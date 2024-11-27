#include "stream_window.h"

#include <fmt/core.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstbus.h>
#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstparse.h>
#include <gst/gstpipeline.h>
#include <gst/gstsample.h>
#include <gst/gststructure.h>
#include <gst/video/video-info.h>
#include <qlogging.h>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QDir>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSettings>
#include <QString>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "xdaq_camera_control.h"
#include "xdaqvc/xvc.h"



namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace
{
constexpr auto SAVE_PATHS = "save_paths";
constexpr auto TRIGGER_ON = "trigger_on";
constexpr auto DIR_DATE = "dir_date";
constexpr auto DIR_NAME = "dir_name";
constexpr auto DIGITAL_CHANNEL = "digital_channel";
constexpr auto TRIGGER_CONDITION = "trigger_condition";
constexpr auto TRIGGER_DURATION = "trigger_duration";
constexpr auto IP = "192.168.177.100";

void set_state(GstElement *element, GstState state)
{
    auto ret = gst_element_set_state(element, state);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_error(
            "Failed to change the element %s state to: %s",
            GST_ELEMENT_NAME(element),
            gst_element_state_get_name(state)
        );
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
    }
}

void create_directory(const QString &save_path, const QString &dir_name)
{
    auto path = fs::path(save_path.toStdString()) / dir_name.toStdString();

    if (!fs::exists(path)) {
        std::error_code ec;
        spdlog::info("create_directory = {}", path.generic_string());
        if (!fs::create_directories(path, ec)) {
            spdlog::info(
                "Failed to create directory: {}. Error: {}", path.generic_string(), ec.message()
            );
        }
    }
}
}  // namespace

GstFlowReturn draw_image(GstAppSink *sink, void *user_data)
{
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample(
        gst_app_sink_pull_sample(sink), gst_sample_unref
    );
    if (!sample) return GST_FLOW_OK;

    auto buffer = gst_sample_get_buffer(sample.get());
    GstMapInfo info;  // contains the actual image
    if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
        std::unique_ptr<GstVideoInfo, decltype(&gst_video_info_free)> video_info(
            gst_video_info_new(), gst_video_info_free
        );
        if (!gst_video_info_from_caps(video_info.get(), gst_sample_get_caps(sample.get()))) {
            g_warning("Failed to parse video info");
            gst_buffer_unmap(buffer, &info);
            return GST_FLOW_ERROR;
        }
        auto caps = gst_sample_get_caps(sample.get());
        auto structure = gst_caps_get_structure(caps, 0);
        auto width = g_value_get_int(gst_structure_get_value(structure, "width"));
        auto height = g_value_get_int(gst_structure_get_value(structure, "height"));

        auto stream_window = (StreamWindow *) user_data;
        auto image_data = static_cast<unsigned char *>(info.data);
        QImage image(image_data, width, height, QImage::Format::Format_RGB888);

        auto current_time = GST_BUFFER_PTS(buffer);
        auto fps = 0.0;
        if (stream_window->frame_time != GST_CLOCK_TIME_NONE) {
            auto diff = current_time - stream_window->frame_time;
            fps = GST_SECOND / (double) diff;
        }
        stream_window->frame_time = current_time;

        auto xdaqmetadata =
            stream_window->handler.get()->safe_deque.check_pts_pop_timestamp(buffer->pts);
        auto metadata = xdaqmetadata.value_or(XDAQFrameData{0, 0, 0, 0, 0, 0});

        QMetaObject::invokeMethod(
            stream_window,
            [stream_window, image, fps, metadata]() {
                stream_window->set_image(image);
                stream_window->set_fps(fps);
                stream_window->set_metadata(metadata);
            },
            Qt::QueuedConnection
        );

        QSettings settings("KonteX", "VC");
        settings.beginGroup(stream_window->camera->get_name());
        if (!settings.value(TRIGGER_ON, true).toBool()) {
            gst_buffer_unmap(buffer, &info);
            return GST_FLOW_OK;
        }
        auto save_path = settings.value(SAVE_PATHS, QDir::currentPath()).toStringList().first();
        auto dir_name = settings.value(DIR_DATE, true).toBool()
                            ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss")
                            : settings.value(DIR_NAME).toString();
        auto digital_channel = settings.value(DIGITAL_CHANNEL, 1).toInt();
        auto trigger_condition = settings.value(TRIGGER_CONDITION, 0).toInt();
        auto trigger_duration = settings.value(TRIGGER_DURATION, 10).toInt();
        settings.endGroup();

        if (digital_channel == metadata.ttl_in && metadata.ttl_in >= 1 && metadata.ttl_in <= 32) {
            stream_window->status =
                stream_window->status == StreamWindow::Record::KeepNo
                    ? stream_window->status = StreamWindow::Record::Start  // 0 -> 1
                    : stream_window->status = StreamWindow::Record::Keep;  // 1 -> 1
        } else {
            stream_window->status =
                stream_window->status == StreamWindow::Record::Keep
                    ? stream_window->status = StreamWindow::Record::Stop     // 1 -> 0
                    : stream_window->status = StreamWindow::Record::KeepNo;  // 0 -> 0
        }

        if (trigger_condition == 1) {
            if (stream_window->status == StreamWindow::Record::Start) {
                create_directory(save_path, dir_name);

                QMetaObject::invokeMethod(stream_window, [stream_window, save_path, dir_name]() {
                    auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                    stream_window->camera->get_name();

                    if (stream_window->camera->get_current_cap().find("image/jpeg") !=
                        std::string::npos) {
                        xvc::start_jpeg_recording(GST_PIPELINE(stream_window->pipeline), filepath);
                    } else {
                        stream_window->start_h265_recording(filepath);
                    }
                    // HACK
                    auto main_window = qobject_cast<XDAQCameraControl *>(
                        stream_window->parentWidget()->parentWidget()
                    );
                    main_window->cameras_list->setDisabled(true);
                    main_window->record_button->setDisabled(true);
                    main_window->record_button->setText("STOP");
                    main_window->record_time->setText("00:00:00");
                    main_window->timer->start(1000);
                    main_window->elapsed_time = 0;
                });
            } else if (stream_window->status == StreamWindow::Record::Stop) {
                QMetaObject::invokeMethod(stream_window, [stream_window]() {
                    if (stream_window->camera->get_current_cap().find("image/jpeg") !=
                        std::string::npos) {
                        xvc::stop_jpeg_recording(GST_PIPELINE(stream_window->pipeline));
                    } else {
                        xvc::stop_h265_recording(GST_PIPELINE(stream_window->pipeline));
                    }
                    auto xdaq_camera_control = qobject_cast<XDAQCameraControl *>(
                        stream_window->parentWidget()->parentWidget()
                    );
                    xdaq_camera_control->timer->stop();
                    xdaq_camera_control->cameras_list->setDisabled(false);
                    xdaq_camera_control->record_button->setDisabled(false);
                    xdaq_camera_control->record_button->setText("REC");
                });
            }
        } else if (trigger_condition == 2) {
            if (stream_window->status == StreamWindow::Record::Start && !stream_window->recording) {
                create_directory(save_path, dir_name);
                stream_window->recording = true;

                QMetaObject::invokeMethod(stream_window, [stream_window, save_path, dir_name]() {
                    auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                    stream_window->camera->get_name();

                    if (stream_window->camera->get_current_cap().find("image/jpeg") !=
                        std::string::npos) {
                        xvc::start_jpeg_recording(GST_PIPELINE(stream_window->pipeline), filepath);
                    } else {
                        stream_window->start_h265_recording(filepath);
                    }
                    // HACK
                    auto main_window = qobject_cast<XDAQCameraControl *>(
                        stream_window->parentWidget()->parentWidget()
                    );
                    main_window->cameras_list->setDisabled(true);
                    main_window->record_button->setDisabled(true);
                    main_window->record_button->setText("STOP");
                    main_window->record_time->setText("00:00:00");
                    main_window->timer->start(1000);
                    main_window->elapsed_time = 0;
                });
            } else if (stream_window->status == StreamWindow::Record::Start &&
                       stream_window->recording) {
                stream_window->recording = false;
                QMetaObject::invokeMethod(stream_window, [stream_window]() {
                    if (stream_window->camera->get_current_cap().find("image/jpeg") !=
                        std::string::npos) {
                        xvc::stop_jpeg_recording(GST_PIPELINE(stream_window->pipeline));
                    } else {
                        xvc::stop_h265_recording(GST_PIPELINE(stream_window->pipeline));
                    }
                    auto main_window = qobject_cast<XDAQCameraControl *>(
                        stream_window->parentWidget()->parentWidget()
                    );
                    main_window->timer->stop();
                    main_window->cameras_list->setDisabled(false);
                    main_window->record_button->setDisabled(false);
                    main_window->record_button->setText("REC");
                });
            }
        } else if (trigger_condition == 3) {
            if (stream_window->status == StreamWindow::Record::Start && !stream_window->recording) {
                create_directory(save_path, dir_name);

                QMetaObject::invokeMethod(
                    stream_window,
                    [stream_window, save_path, dir_name, trigger_duration]() {
                        stream_window->recording = true;

                        auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                        stream_window->camera->get_name();

                        if (stream_window->camera->get_current_cap().find("image/jpeg") !=
                            std::string::npos) {
                            xvc::start_jpeg_recording(
                                GST_PIPELINE(stream_window->pipeline), filepath
                            );
                        } else {
                            stream_window->start_h265_recording(filepath);
                        }
                        // HACK
                        auto main_window = qobject_cast<XDAQCameraControl *>(
                            stream_window->parentWidget()->parentWidget()
                        );
                        main_window->cameras_list->setDisabled(true);
                        main_window->record_button->setDisabled(true);
                        main_window->record_button->setText("STOP");
                        main_window->record_time->setText("00:00:00");
                        main_window->timer->start(1000);
                        main_window->elapsed_time = 0;

                        QTimer::singleShot(trigger_duration, [main_window, stream_window]() {
                            if (stream_window->camera->get_current_cap().find("image/jpeg") !=
                                std::string::npos) {
                                xvc::stop_jpeg_recording(GST_PIPELINE(stream_window->pipeline));
                            } else {
                                xvc::stop_h265_recording(GST_PIPELINE(stream_window->pipeline));
                            }
                            stream_window->recording = false;

                            main_window->timer->stop();
                            main_window->cameras_list->setDisabled(false);
                            main_window->record_button->setDisabled(false);
                            main_window->record_button->setText("REC");
                        });
                    }
                );
            }
        }
        gst_buffer_unmap(buffer, &info);
    }
    return GST_FLOW_OK;
}

StreamWindow::StreamWindow(Camera *_camera, QWidget *parent)
    : QDockWidget(parent),
      pipeline(nullptr),
      frame_time(GST_CLOCK_TIME_NONE),
      status(StreamWindow::Record::KeepNo),
      recording(false),
      pause(false)
{
    camera = _camera;

    handler = std::make_unique<H265MetadataHandler>();

    setFixedSize(480, 360);
    setFeatures(features() & ~QDockWidget::DockWidgetClosable);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setWindowTitle(QString::fromStdString(camera->get_name()));

    icon = new QLabel(this);
    icon->setAlignment(Qt::AlignCenter);
    icon->setAttribute(Qt::WA_TranslucentBackground);

    fade = new QPropertyAnimation(icon, "opacity");
    fade->setDuration(300);

    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect(icon);
    icon->setGraphicsEffect(opacity);

    connect(fade, &QPropertyAnimation::valueChanged, [opacity](const QVariant &value) {
        opacity->setOpacity(value.toDouble());
    });

    pipeline = gst_pipeline_new("xdaqvc");
    if (!pipeline) {
        g_error("Pipeline could be created");
        return;
    }
    auto uri = fmt::format("{}:{}", IP, camera->get_port());
    if (camera->get_current_cap().find("image/jpeg") != std::string::npos) {
        xvc::setup_jpeg_srt_stream(GST_PIPELINE(pipeline), uri);

        auto parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> src_pad(
            gst_element_get_static_pad(parser, "src"), gst_object_unref
        );
        gst_pad_add_probe(
            src_pad.get(), GST_PAD_PROBE_TYPE_BUFFER, parse_jpeg_metadata, handler.get(), NULL
        );

        const int max_fps = 60;
        auto camera_caps = camera->get_caps();
        auto fps = (float) camera_caps[0].fps_n / camera_caps[0].fps_d;
        if (fps > max_fps) {
            bus_thread_running = true;
            bus_thread = std::thread(&StreamWindow::poll_bus_messages, this);
        }
    } else if (camera->get_id() == -1) {
        xvc::mock_high_frame_rate(GST_PIPELINE(pipeline), uri);
    } else {
        xvc::setup_h265_srt_stream(GST_PIPELINE(pipeline), uri);

        auto parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> src_pad(
            gst_element_get_static_pad(parser, "src"), gst_object_unref
        );
        gst_pad_add_probe(
            src_pad.get(), GST_PAD_PROBE_TYPE_BUFFER, parse_h265_metadata, handler.get(), NULL
        );
    }

    GstAppSinkCallbacks callbacks = {NULL};
    callbacks.new_sample = draw_image;
    auto appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, NULL);
}

StreamWindow::~StreamWindow()
{
    camera->stop();

    bus_thread_running = false;
    if (bus_thread.joinable()) {
        bus_thread.join();
    }

    set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

void StreamWindow::closeEvent(QCloseEvent *e)
{
    e->accept();
    handler->last_frame_buffers.clear();
}

void StreamWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // painter.setRenderHint(QPainter::Antialiasing);
    // painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, width(), height()), image, image.rect());
    painter.setPen(QPen(Qt::white));
    if (metadata.ttl_in >= 1 && metadata.ttl_in <= 32) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(181, 157, 99)));

        QPointF DI(16, 16);
        painter.setOpacity(0.5);
        painter.drawEllipse(DI, 10, 10);

        QRectF text(DI.x() - 8, DI.y() - 8, 15, 15);
        painter.setOpacity(1);
        painter.setPen(QPen(Qt::black));
        painter.drawText(text, Qt::AlignCenter, QString::number(metadata.ttl_in));

        painter.setPen(QPen(Qt::white));
    }
    painter.drawText(
        QRect(10, height() - 60, width() / 2, height() - 60),
        QString::fromStdString(fmt::format("XDAQ Time {:08x}", metadata.fpga_timestamp))
    );
    painter.drawText(
        QRect(10, height() - 30, width() / 2, height() - 30),
        QString::fromStdString(fmt::format("Ephys Time {:04x}", metadata.rhythm_timestamp))
    );
    painter.drawText(
        QRect(width() - 130, height() - 30, width(), height() - 30),
        QString::fromStdString(fmt::format("DO word {:04x}", metadata.ttl_out))
    );
    painter.drawText(
        QRect(width() - 130, height() - 60, width(), height() - 30),
        QString::fromStdString(fmt::format("FPS {:.2f}", fps_text))
    );
}

void StreamWindow::mousePressEvent(QMouseEvent *e)
{
    pause = !pause;

    QPixmap pixmap;
    pixmap = pause ? style()->standardPixmap(QStyle::SP_MediaPause)
                   : style()->standardPixmap(QStyle::SP_MediaPlay);
    pixmap = pixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap white_pixmap(pixmap.size());
    white_pixmap.fill(Qt::transparent);
    QPainter painter(&white_pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(0, 0, pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(white_pixmap.rect(), Qt::white);
    painter.end();

    icon->setPixmap(white_pixmap);
    icon->resize(pixmap.size());
    icon->move((width() - icon->width()) / 2, (height() - icon->height()) / 2);

    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start();
    QTimer::singleShot(600, [this]() {
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->start();
    });
}

void StreamWindow::set_image(const QImage &_image)
{
    if (!pause) {
        image = _image;
        update();
    }
}

void StreamWindow::set_metadata(const XDAQFrameData &_metadata)
{
    if (!pause) {
        metadata = _metadata;
        update();
    }
}

void StreamWindow::set_fps(const double _fps)
{
    if (!pause) {
        fps_text = _fps;
        update();
    }
}

void StreamWindow::play()
{
    camera->start();
    if (pipeline) {
        set_state(pipeline, GST_STATE_PLAYING);
    }
}

void StreamWindow::start_h265_recording(fs::path &filepath)
{
    xvc::start_h265_recording(GST_PIPELINE(pipeline), filepath);

    auto tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    auto src_pad = std::unique_ptr<GstPad, decltype(&gst_object_unref)>(
        gst_element_get_static_pad(tee, "src_1"), gst_object_unref
    );

    spdlog::info(
        "Pushing I-frame with PTS: {}", GST_BUFFER_PTS(handler->last_frame_buffers.front())
    );
    gst_pad_push(src_pad.get(), gst_buffer_ref(handler->last_frame_buffers.front()));
    for (auto buffer : handler->last_frame_buffers) {
        spdlog::info("Pushing frame with PTS: {}", GST_BUFFER_PTS(buffer));
        gst_pad_push(src_pad.get(), gst_buffer_ref(buffer));
    }
}

void StreamWindow::poll_bus_messages()
{
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    while (bus_thread_running) {
        GstMessage *msg = gst_bus_timed_pop(bus, 100 * GST_MSECOND);
        if (msg != nullptr) {
            switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug;
                gst_message_parse_error(msg, &err, &debug);
                spdlog::error("Error: {}", err->message);
                g_error_free(err);
                g_free(debug);
                break;
            }
            case GST_MESSAGE_WARNING: {
                GError *err;
                gchar *debug;
                gst_message_parse_warning(msg, &err, &debug);
                spdlog::warn("Warning: {}", err->message);
                g_error_free(err);
                g_free(debug);
                break;
            }
            default: break;
            }
            gst_message_unref(msg);
        }
    }
    gst_object_unref(bus);
}