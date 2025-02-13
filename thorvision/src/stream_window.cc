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
#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStyle>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "stream_mainwindow.h"
#include "xdaqvc/xvc.h"


namespace fs = std::filesystem;
using namespace std::chrono_literals;


namespace
{
#ifdef TTL
auto constexpr CONTINUOUS = "continuous";
auto constexpr TRIGGER_ON = "trigger_on";
auto constexpr DIGITAL_CHANNEL = "digital_channel";
auto constexpr TRIGGER_CONDITION = "trigger_condition";
auto constexpr TRIGGER_DURATION = "trigger_duration";

auto constexpr MAX_SIZE_TIME = "max_size_time";
auto constexpr MAX_FILES = "max_files";

auto constexpr SAVE_PATHS = "save_paths";
auto constexpr DIR_DATE = "dir_date";
auto constexpr DIR_NAME = "dir_name";

void create_directory(const QString &save_path, const QString &dir_name)
{
    auto path = fs::path(save_path.toStdString()) / dir_name.toStdString();
    if (!fs::exists(path)) {
        std::error_code ec;
        spdlog::info("Create directory {}", path.generic_string());
        if (!fs::create_directories(path, ec)) {
            spdlog::info(
                "Failed to create directory: {}. Error: {}", path.generic_string(), ec.message()
            );
        }
    }
}
#endif

auto constexpr IP = "192.168.177.100";

auto constexpr VIDEO_RAW = "video/x-raw";
auto constexpr VIDEO_MJPEG = "image/jpeg";

void set_state(GstElement *element, GstState state)
{
    spdlog::info("Set pipeline status to {}", gst_element_state_get_name(state));
    auto ret = gst_element_set_state(element, state);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
        spdlog::error(
            "Failed to change the element {} state to: {}",
            GST_ELEMENT_NAME(element),
            gst_element_state_get_name(state)
        );
    }
}

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
            spdlog::critical("Failed to parse video info");
            gst_buffer_unmap(buffer, &info);
            return GST_FLOW_ERROR;
        }
        auto caps = gst_sample_get_caps(sample.get());
        auto structure = gst_caps_get_structure(caps, 0);
        auto width = static_cast<int>(g_value_get_int(gst_structure_get_value(structure, "width")));
        auto height =
            static_cast<int>(g_value_get_int(gst_structure_get_value(structure, "height")));

        auto stream_window = (StreamWindow *) user_data;
        auto image_data = static_cast<unsigned char *>(info.data);
        QImage image(image_data, width, height, QImage::Format::Format_RGB888);

        auto xdaqmetadata =
            stream_window->_handler.get()->safe_deque.check_pts_pop_timestamp(buffer->pts);
        auto metadata = xdaqmetadata.value_or(XDAQFrameData{0, 0, 0, 0, 0, 0});

        QMetaObject::invokeMethod(
            stream_window,
            [stream_window, image, metadata]() {
                stream_window->set_image(image);
                stream_window->set_metadata(metadata);
            },
            Qt::QueuedConnection
        );

#ifdef TTL
        QSettings settings("KonteX Neuroscience", "Thor Vision");
        auto continuous = settings.value(CONTINUOUS, true).toBool();

        auto max_size_time = settings.value(MAX_SIZE_TIME, 0).toInt();
        auto max_files = settings.value(MAX_FILES, 10).toInt();

        auto save_path =
            settings
                .value(
                    SAVE_PATHS, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                )
                .toStringList()
                .first();
        auto dir_name = settings.value(DIR_DATE, true).toBool()
                            ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss")
                            : settings.value(DIR_NAME).toString();

        settings.beginGroup(stream_window->_camera->name());
        if (!settings.value(TRIGGER_ON, true).toBool()) {
            gst_buffer_unmap(buffer, &info);
            return GST_FLOW_OK;
        }
        auto digital_channel = settings.value(DIGITAL_CHANNEL, 0).toUInt();
        auto trigger_condition = settings.value(TRIGGER_CONDITION, 0).toUInt();
        auto trigger_duration = settings.value(TRIGGER_DURATION, 1).toUInt();
        settings.endGroup();

        if (digital_channel + 1 == metadata.ttl_in && metadata.ttl_in >= 1 &&
            metadata.ttl_in <= 32) {
            stream_window->_status =
                stream_window->_status == StreamWindow::Record::KeepNo
                    ? stream_window->_status = StreamWindow::Record::Start  // 0 -> 1
                    : stream_window->_status = StreamWindow::Record::Keep;  // 1 -> 1
        } else {
            stream_window->_status =
                stream_window->_status == StreamWindow::Record::Keep
                    ? stream_window->_status = StreamWindow::Record::Stop     // 1 -> 0
                    : stream_window->_status = StreamWindow::Record::KeepNo;  // 0 -> 0
        }

        // TODO: UGLY HACK
        auto main_window =
            qobject_cast<XDAQCameraControl *>(stream_window->parentWidget()->parentWidget());

        if (trigger_condition == 0) {
            if (stream_window->_status == StreamWindow::Record::Start) {
                create_directory(save_path, dir_name);

                QMetaObject::invokeMethod(stream_window, [=]() {
                    auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                    stream_window->_camera->name();

                    if (stream_window->_camera->current_cap().find(VIDEO_MJPEG) !=
                            std::string::npos ||
                        stream_window->_camera->current_cap().find(VIDEO_RAW) !=
                            std::string::npos) {
                        xvc::start_jpeg_recording(
                            GST_PIPELINE(stream_window->_pipeline.get()),
                            filepath,
                            continuous,
                            max_size_time,
                            max_files
                        );
                    } else {
                        // TODO: disable h265 for now
                        stream_window->start_h265_recording(
                            filepath, continuous, max_size_time, max_files
                        );
                    }
                    main_window->_timer->start(1000);
                    main_window->_elapsed_time = 0;
                    main_window->_camera_list->setDisabled(true);
                    main_window->_record_button->setDisabled(true);
                    main_window->_record_button->setText("STOP");
                    main_window->_record_time->setText("00:00:00");
                    main_window->_recording = true;
                });
            } else if (stream_window->_status == StreamWindow::Record::Stop) {
                QMetaObject::invokeMethod(stream_window, [=]() {
                    if (stream_window->_camera->current_cap().find(VIDEO_MJPEG) !=
                            std::string::npos ||
                        stream_window->_camera->current_cap().find(VIDEO_RAW) !=
                            std::string::npos) {
                        xvc::stop_jpeg_recording(GST_PIPELINE(stream_window->_pipeline.get()));
                    } else {
                        // TODO: disable h265 for now
                        xvc::stop_h265_recording(GST_PIPELINE(stream_window->_pipeline.get()));
                    }
                    main_window->_timer->stop();
                    main_window->_camera_list->setDisabled(false);
                    main_window->_record_button->setDisabled(false);
                    main_window->_record_button->setText("REC");
                });
            }
        } else if (trigger_condition == 1) {
            if (stream_window->_status == StreamWindow::Record::Start && !main_window->_recording) {
                create_directory(save_path, dir_name);

                QMetaObject::invokeMethod(stream_window, [=]() {
                    auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                    stream_window->_camera->name();

                    if (stream_window->_camera->current_cap().find(VIDEO_MJPEG) !=
                            std::string::npos ||
                        stream_window->_camera->current_cap().find(VIDEO_RAW) !=
                            std::string::npos) {
                        xvc::start_jpeg_recording(
                            GST_PIPELINE(stream_window->_pipeline.get()),
                            filepath,
                            continuous,
                            max_size_time,
                            max_files
                        );
                    } else {
                        // TODO: disable h265 for now
                        stream_window->start_h265_recording(
                            filepath, continuous, max_size_time, max_files
                        );
                    }
                    main_window->_timer->start(1000);
                    main_window->_elapsed_time = 0;
                    main_window->_camera_list->setDisabled(true);
                    main_window->_record_button->setDisabled(true);
                    main_window->_record_button->setText("STOP");
                    main_window->_record_time->setText("00:00:00");
                    main_window->_recording = true;
                });
            } else if (stream_window->_status == StreamWindow::Record::Start &&
                       main_window->_recording) {
                QMetaObject::invokeMethod(stream_window, [=]() {
                    if (stream_window->_camera->current_cap().find(VIDEO_MJPEG) !=
                            std::string::npos ||
                        stream_window->_camera->current_cap().find(VIDEO_RAW) !=
                            std::string::npos) {
                        xvc::stop_jpeg_recording(GST_PIPELINE(stream_window->_pipeline.get()));
                    } else {
                        // TODO: disable h265 for now
                        xvc::stop_h265_recording(GST_PIPELINE(stream_window->_pipeline.get()));
                    }
                    main_window->_timer->stop();
                    main_window->_camera_list->setDisabled(false);
                    main_window->_record_button->setDisabled(false);
                    main_window->_record_button->setText("REC");
                    main_window->_recording = false;
                });
            }
        } else if (trigger_condition == 2) {
            if (stream_window->_status == StreamWindow::Record::Start && !main_window->_recording) {
                create_directory(save_path, dir_name);

                QMetaObject::invokeMethod(stream_window, [=]() {
                    auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                    stream_window->_camera->name();

                    if (stream_window->_camera->current_cap().find(VIDEO_MJPEG) !=
                            std::string::npos ||
                        stream_window->_camera->current_cap().find(VIDEO_RAW) !=
                            std::string::npos) {
                        xvc::start_jpeg_recording(
                            GST_PIPELINE(stream_window->_pipeline.get()),
                            filepath,
                            continuous,
                            max_size_time,
                            max_files
                        );
                    } else {
                        // TODO: disable h265 for now
                        stream_window->start_h265_recording(
                            filepath, continuous, max_size_time, max_files
                        );
                    }
                    main_window->_elapsed_time = 0;
                    main_window->_timer->start(1000);
                    main_window->_camera_list->setDisabled(true);
                    main_window->_record_button->setDisabled(true);
                    main_window->_record_button->setText("STOP");
                    main_window->_record_time->setText("00:00:00");
                    main_window->_recording = true;

                    QTimer::singleShot(trigger_duration * 1000, [main_window, stream_window]() {
                        if (stream_window->_camera->current_cap().find(VIDEO_MJPEG) !=
                                std::string::npos ||
                            stream_window->_camera->current_cap().find(VIDEO_RAW) !=
                                std::string::npos) {
                            xvc::stop_jpeg_recording(GST_PIPELINE(stream_window->_pipeline.get()));
                        } else {
                            // TODO: disable h265 for now
                            xvc::stop_h265_recording(GST_PIPELINE(stream_window->_pipeline.get()));
                        }
                        main_window->_timer->stop();
                        main_window->_camera_list->setDisabled(false);
                        main_window->_record_button->setDisabled(false);
                        main_window->_record_button->setText("REC");
                        main_window->_recording = false;
                    });
                });
            }
        }
#endif
        gst_buffer_unmap(buffer, &info);
    }
    return GST_FLOW_OK;
}
}  // namespace


StreamWindow::StreamWindow(Camera *camera, QWidget *parent)
    : QDockWidget(parent),
      _camera(nullptr),
      _pipeline(nullptr, gst_object_unref),
      _status(StreamWindow::Record::KeepNo),
      _pause(false)
{
    _camera = camera;

    _handler = std::make_unique<MetadataHandler>();

    setFixedSize(480, 360);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setWindowTitle(QString::fromStdString(camera->name()));

    _icon = new QLabel(this);
    _icon->setAlignment(Qt::AlignCenter);
    _icon->setAttribute(Qt::WA_TranslucentBackground);

    auto opacity = new QGraphicsOpacityEffect(_icon);
    _icon->setGraphicsEffect(opacity);
    _fade = new QPropertyAnimation(_icon);
    _pipeline = {gst_pipeline_new(camera->name().c_str()), gst_object_unref};

    auto uri = fmt::format("{}:{}", IP, camera->port());
    if (camera->current_cap().find(VIDEO_MJPEG) != std::string::npos ||
        camera->current_cap().find(VIDEO_RAW) != std::string::npos) {
        xvc::setup_jpeg_srt_stream(GST_PIPELINE(_pipeline.get()), uri);

        auto parser = gst_bin_get_by_name(GST_BIN(_pipeline.get()), "parser");
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> src_pad(
            gst_element_get_static_pad(parser, "src"), gst_object_unref
        );
        gst_pad_add_probe(
            src_pad.get(), GST_PAD_PROBE_TYPE_BUFFER, parse_jpeg_metadata, _handler.get(), nullptr
        );

        _bus_thread_running = true;
        _bus_thread = std::jthread(&StreamWindow::poll_bus_messages, this);
    } else if (camera->id() == -1) {
        xvc::mock_camera(GST_PIPELINE(_pipeline.get()), uri);
    } else {
        // TODO: disable h265 for now
        xvc::setup_h265_srt_stream(GST_PIPELINE(_pipeline.get()), uri);

        auto parser = gst_bin_get_by_name(GST_BIN(_pipeline.get()), "parser");
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> src_pad(
            gst_element_get_static_pad(parser, "src"), gst_object_unref
        );
        gst_pad_add_probe(
            src_pad.get(), GST_PAD_PROBE_TYPE_BUFFER, parse_h265_metadata, _handler.get(), nullptr
        );
    }

    GstAppSinkCallbacks callbacks = {nullptr, nullptr, draw_image, nullptr, nullptr, {nullptr}};
    auto appsink = gst_bin_get_by_name(GST_BIN(_pipeline.get()), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);
}

StreamWindow::~StreamWindow()
{
    _bus_thread_running = false;
    set_state(_pipeline.get(), GST_STATE_NULL);
}

void StreamWindow::closeEvent(QCloseEvent *e)
{
    deleteLater();

    _camera->stop();
    _handler->last_frame_buffers.clear();

    auto stream_mainwindow = qobject_cast<StreamMainWindow *>(parentWidget());
    stream_mainwindow->removeDockWidget(this);

    emit window_close();

    e->accept();
}

void StreamWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, width(), height()), _image, _image.rect());
    painter.setPen(QPen(Qt::white));
    if (_metadata.ttl_in >= 1 && _metadata.ttl_in <= 32) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(181, 157, 99)));

        QPointF DI(16, 16);
        painter.setOpacity(0.5);
        painter.drawEllipse(DI, 10, 10);

        QRectF text(DI.x() - 8, DI.y() - 8, 15, 15);
        painter.setOpacity(1);
        painter.setPen(QPen(Qt::black));
        painter.drawText(text, Qt::AlignCenter, QString::number(_metadata.ttl_in));

        painter.setPen(QPen(Qt::white));
    }
    painter.drawText(
        QRect(10, height() - 60, width() / 2, height() - 60),
        QString::fromStdString(fmt::format("XDAQ Time {:08x}", _metadata.fpga_timestamp))
    );
    painter.drawText(
        QRect(10, height() - 30, width() / 2, height() - 30),
        QString::fromStdString(fmt::format("Ephys Time {:04x}", _metadata.rhythm_timestamp))
    );
    painter.drawText(
        QRect(width() - 130, height() - 30, width(), height() - 30),
        QString::fromStdString(fmt::format("DO word {:04x}", _metadata.ttl_out))
    );
}

void StreamWindow::mousePressEvent(QMouseEvent *)
{
    _pause = !_pause;

    auto pixmap = _pause ? style()->standardPixmap(QStyle::SP_MediaPause)
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

    _icon->setPixmap(white_pixmap);
    _icon->resize(pixmap.size());
    _icon->move((width() - _icon->width()) / 2, (height() - _icon->height()) / 2);

    if (_fade->state() == QAbstractAnimation::Running) {
        _fade->stop();
    }

    auto opacity = qobject_cast<QGraphicsOpacityEffect *>(_icon->graphicsEffect());
    if (!opacity) {
        opacity = new QGraphicsOpacityEffect(_icon);
        _icon->setGraphicsEffect(opacity);
    }

    _fade->setTargetObject(opacity);
    _fade->setPropertyName("opacity");
    _fade->setDuration(600);
    _fade->setStartValue(1.0);
    _fade->setEndValue(0.0);
    _fade->start();
}

void StreamWindow::set_image(const QImage &image)
{
    if (!_pause) {
        _image = image;
        update();
    }
}

void StreamWindow::set_metadata(const XDAQFrameData &metadata)
{
    if (!_pause) {
        _metadata = metadata;
        update();
    }
}

void StreamWindow::play()
{
    _camera->start();
    set_state(_pipeline.get(), GST_STATE_PLAYING);
}

void StreamWindow::stop()
{
    _camera->stop();
    set_state(_pipeline.get(), GST_STATE_NULL);
}

void StreamWindow::start_h265_recording(
    fs::path &filepath, bool continuous, int max_size_time, int max_files
)
{
    xvc::start_h265_recording(
        GST_PIPELINE(_pipeline.get()), filepath, continuous, max_size_time, max_files
    );

    auto tee = gst_bin_get_by_name(GST_BIN(_pipeline.get()), "t");
    auto src_pad = std::unique_ptr<GstPad, decltype(&gst_object_unref)>(
        gst_element_get_static_pad(tee, "src_1"), gst_object_unref
    );

    spdlog::debug(
        "Pushing I-frame with PTS: {}", GST_BUFFER_PTS(_handler->last_frame_buffers.front())
    );
    gst_pad_push(src_pad.get(), gst_buffer_ref(_handler->last_frame_buffers.front()));
    for (auto buffer : _handler->last_frame_buffers) {
        spdlog::debug("Pushing frame with PTS: {}", GST_BUFFER_PTS(buffer));
        gst_pad_push(src_pad.get(), gst_buffer_ref(buffer));
    }
}

void StreamWindow::poll_bus_messages()
{
    std::unique_ptr<GstBus, decltype(&gst_object_unref)> bus(
        gst_pipeline_get_bus(GST_PIPELINE(_pipeline.get())), gst_object_unref
    );
    while (_bus_thread_running) {
        std::unique_ptr<GstMessage, decltype(&gst_message_unref)> msg(
            gst_bus_timed_pop(bus.get(), 100 * GST_MSECOND), gst_message_unref
        );
        if (msg) {
            switch (GST_MESSAGE_TYPE(msg.get())) {
            case GST_MESSAGE_ERROR: {
                GError *err = nullptr;
                gchar *debug = nullptr;
                gst_message_parse_error(msg.get(), &err, &debug);
                if (err) {
                    spdlog::error("Error: {}", err->message);
                    g_error_free(err);
                }
                if (debug) {
                    spdlog::debug("Debug: {}", debug);
                    g_free(debug);
                }
                break;
            }
            case GST_MESSAGE_WARNING: {
                GError *err = nullptr;
                gchar *debug = nullptr;
                gst_message_parse_warning(msg.get(), &err, &debug);
                if (err) {
                    spdlog::warn("Warning: {}", err->message);
                    g_error_free(err);
                }
                if (debug) {
                    spdlog::debug("Debug: {}", debug);
                    g_free(debug);
                }
                break;
            }
            default: break;
            }
        }
    }
}