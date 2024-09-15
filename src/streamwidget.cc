#include "streamwidget.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qglobal.h>
#include <QtCore/qlogging.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstring.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qimage.h>
#include <QtGui/qpixmap.h>
#include <QtMultimedia/qmediacontent.h>
#include <QtMultimedia/qvideoframe.h>
#include <QtWidgets/qcombobox.h>
#include <_types/_uint64_t.h>
#include <cpr/api.h>
#include <cpr/body.h>
#include <cpr/cprtypes.h>
#include <cpr/parameters.h>
#include <cpr/payload.h>
#include <fmt/core.h>
#include <gst/app/gstappsink.h>
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <gst/gstenumtypes.h>
#include <gst/gstmessage.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstpipeline.h>
#include <gst/gststructure.h>
#include <gst/video/videooverlay.h>
#include <malloc/_malloc.h>
#include <sys/signal.h>
#include <sys/socket.h>

#include <QMediaPlayer>
#include <QVideoFrame>
#include <QVideoWidget>
#include <array>
#include <iostream>

#include "../libxvc.h"
#include "mainwindow.h"
#include "safedeque.h"
// #include "myvideointerface.h"
#include <QAbstractVideoSurface>
// #include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

using nlohmann::json;
using namespace std::chrono_literals;

void set_state(GstElement *element, GstState state)
{
    GstStateChangeReturn ret = gst_element_set_state(element, state);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_error(
            "Failed to change the element %s state to: %s",
            GST_ELEMENT_NAME(element),
            gst_element_state_get_name(state)
        );
        // std::string msg =
        //     std::string("Failed to change the element state to: ") + std::to_string(state);
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
        // throw std::runtime_error(msg);
    }
}

auto create_camera_default_combobox()
{
    qInfo("create_camera_default_combobox");
    QSettings settings;
    QComboBox *camera_settings = new QComboBox;
    camera_settings->setStyleSheet("color: white");

    std::string cameras = xvc::list_cameras("192.168.50.241:8000");
    if (cameras.empty()) {
        camera_settings->setDisabled(true);
    } else {
        json cameras_json = json::parse(cameras);
        for (const auto &camera : cameras_json) {
            fmt::println("{}", camera.dump(2));
            // if (camera["idle"] == "") {
            //     for (const auto &cap : camera["capabilities"]) {
            //         camera_settings->addItem(QString::fromStdString(cap.get<std::string>()));
            //     }
            //     // settings.value
            // }
            // if (camera["id"]) {
            //     fmt::println("id");
            // }
        }
    }

    return camera_settings;
}

static GstPadProbeReturn extract_timestamp_from_SEI(
    GstPad *pad, GstPadProbeInfo *info, gpointer user_data
)
{
    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    GstH265Parser *nalu_parser = gst_h265_parser_new();

    GstMapInfo map_info;
    GstH265NalUnit nalu;
    for (int i = 0; i < gst_buffer_n_memory(buffer); ++i) {
        GstMemory *mem_in_buffer = gst_buffer_get_memory(buffer, i);
        gst_memory_map(mem_in_buffer, &map_info, GST_MAP_READ);

        GstH265ParserResult parse_result = gst_h265_parser_identify_nalu_unchecked(
            nalu_parser, map_info.data, 0, map_info.size, &nalu
        );
        // g_print("parse_result: %d\n", parse_result);

        if (parse_result == GST_H265_PARSER_OK || parse_result == GST_H265_PARSER_NO_NAL_END ||
            parse_result == GST_H265_PARSER_NO_NAL) {
            // g_print("type: %d\n", nalu.type);

            if (nalu.type == GST_H265_NAL_PREFIX_SEI || nalu.type == GST_H265_NAL_SUFFIX_SEI) {
                GArray *array = g_array_new(false, false, sizeof(GstH265SEIMessage));
                gst_h265_parser_parse_sei(nalu_parser, &nalu, &array);
                GstH265SEIMessage sei_msg = g_array_index(array, GstH265SEIMessage, 0);
                GstH265RegisteredUserData register_user_data = sei_msg.payload.registered_user_data;
                std::string str((const char *) register_user_data.data, register_user_data.size);

                std::array<uint64_t, 4> timestamp;
                std::memcpy(&timestamp, register_user_data.data, register_user_data.size);

                uint64_t pts = buffer->pts;
                SafeDeque::SafeDeque *safe_deque = (SafeDeque::SafeDeque *) user_data;
                safe_deque->push(pts, timestamp);

                g_array_free(array, true);
            }
        }
    }
    gst_h265_parser_free(nalu_parser);

    return GST_PAD_PROBE_OK;
}

static GstFlowReturn callback(GstAppSink *sink, void *user_data)
{
    GstSample *sample = gst_app_sink_pull_sample(sink);
    // g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);

    if (sample) {
        static guint framecount = 0;
        int pixel_data = -1;

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo info;  // contains the actual image
        if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            GstVideoInfo *video_info = gst_video_info_new();
            if (!gst_video_info_from_caps(video_info, gst_sample_get_caps(sample))) {
                // Could not parse video info (should not happen)
                g_warning("Failed to parse video info");
                return GST_FLOW_ERROR;
            }

            // pointer to the image data
            // unsigned char* data = info.data;

            GstCaps *caps = gst_sample_get_caps(sample);
            GError *err = NULL;

            GstStructure *structure = gst_caps_get_structure(caps, 0);
            const int width = g_value_get_int(gst_structure_get_value(structure, "width"));
            const int height = g_value_get_int(gst_structure_get_value(structure, "height"));
            // const std::string format =
            //     g_value_get_string(gst_structure_get_value(structure, "format"));

            // g_print("format = %s", format);
            // fmt::println("format = {}", format);

            // Get the pixel value of the center pixel
            // int stride = video_info->finfo->bits / 8;
            // unsigned int pixel_offset = video_info->width / 2 * stride +
            //                             video_info->width * video_info->height / 2 * stride;

            // this is only one pixel
            // when dealing with formats like BGRx
            // pixel_data will consist out of
            // pixel_offset   => B
            // pixel_offset+1 => G
            // pixel_offset+2 => R
            // pixel_offset+3 => x

            QImage image =
                QImage((const uchar *) info.data, width, height, QImage::Format::Format_RGB888);

            // QVideoFrame frame = QVideoFrame(snapShot);

            // Create a QVideoFrame from the buffer
            // QVideoFrame::PixelFormat format = QVideoFrame::Format_RGB32;
            // QVideoFrame frame((uchar *) info.data, QSize(width, height));
            // QVideoFrame frame(snapShot);

            // auto timestamp = xvc::safe_deque.check_pts_pop_timestamp(buffer->pts);

            // timestamp
            // fmt::println("sinkpad timestamp = {:x}", timestamp);

            auto widget = (StreamWidget *) user_data;
            auto timestamp = widget->safe_deque->check_pts_pop_timestamp(buffer->pts);

            emit widget->frame_ready(image, timestamp);

            // pixel_data = info.data[pixel_offset];

            gst_buffer_unmap(buffer, &info);
            gst_video_info_free(video_info);
        }

        // GstClockTime timestamp = GST_BUFFER_PTS(buffer);

        // g_print(
        //     "Captured frame %d, Pixel Value=%03d Timestamp=%" GST_TIME_FORMAT "            \r",
        //     framecount,
        //     pixel_data,
        //     GST_TIME_ARGS(timestamp)
        // );
        // framecount++;

        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

StreamWidget::StreamWidget(MainWindow *parent)
    : QDockWidget(nullptr), pipeline(nullptr), streaming(false), recording(false)
{
    qInfo("StreamWidget::StreamWidget");
    // this->resize(640, 480);

    QVBoxLayout *main_layout = new QVBoxLayout;
    QWidget *wrapper = new QWidget;

    wrapper->setLayout(main_layout);

    // auto player = new QMediaPlayer;
    // QVideoWidget *video_widget = new QVideoWidget;
    image_label = new QLabel;
    timestamp_label = new QLabel;
    safe_deque = new SafeDeque::SafeDeque();

    // auto camera_settings = create_camera_default_combobox();

    QSettings settings;
    QComboBox *camera_settings = new QComboBox;
    camera_settings->setStyleSheet("color: white");

    connect(this, &StreamWidget::frame_ready, this, &StreamWidget::display_frame);

    // connect(
    //     camera_settings,
    //     QOverload<int>::of(&QComboBox::currentIndexChanged),
    //     this,
    //     &StreamWidget::select_camera_setting
    // );
    connect(
        camera_settings,
        SIGNAL(currentIndexChanged(QString)),
        this,
        SLOT(select_camera_setting(QString))
    );

    // window->resize(165, window->height());
    // container->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // container->setLayout(layout);
    // container->resize(1024, 840);
    // container->layout()->addWidget(window);

    QHBoxLayout *util_layout = new QHBoxLayout;
    stream_button = new QPushButton(tr("Start"));
    record_button = new QPushButton(tr("Record"));
    QPushButton *select_dir_button = new QPushButton(tr("..."));
    save_path = new QLineEdit;
    QPushButton *parse_button = new QPushButton(tr("Parse"));

    // camera_settings->setDisabled(true);
    // stream_button->setDisabled(true);
    // record_button->setDisabled(true);
    save_path->setDisabled(true);

    stream_button->setFixedWidth(stream_button->sizeHint().width() + 3);
    parse_button->setFixedWidth(parse_button->sizeHint().width() + 3);
    record_button->setFixedWidth(record_button->sizeHint().width() + 3);
    int text_width =
        select_dir_button->fontMetrics().horizontalAdvance(select_dir_button->text()) + 10;
    select_dir_button->setFixedWidth(text_width);
    save_path->setFixedWidth(100);

    connect(stream_button, &QPushButton::clicked, this, &StreamWidget::stream_handler);
    connect(record_button, &QPushButton::clicked, this, &StreamWidget::record_stream);
    connect(select_dir_button, &QPushButton::clicked, this, &StreamWidget::select_dir);
    connect(parse_button, &QPushButton::clicked, this, &StreamWidget::parse_video);

    main_layout->addLayout(util_layout);

    util_layout->addWidget(stream_button);
    util_layout->addWidget(record_button);
    util_layout->addWidget(parse_button);
    util_layout->addWidget(camera_settings);
    util_layout->addWidget(select_dir_button);
    util_layout->addWidget(save_path);

    // video_widget->layout()->addWidget(label);
    wrapper->layout()->addWidget(image_label);
    wrapper->layout()->addWidget(timestamp_label);
    setWidget(wrapper);


    std::string cameras = xvc::list_cameras("192.168.50.241:8000/idle");
    fmt::println("cameras = {}", cameras);

    if (cameras.empty()) {
        camera_settings->setDisabled(true);
        stream_button->setDisabled(true);
        record_button->setDisabled(true);
    } else {
        json cameras_json = json::parse(cameras);
        for (const auto &camera : cameras_json) {
            fmt::println("camera = {}", camera.dump(2));

            // if (camera["status"] == "idle") {
            camera_id = camera["id"];

            camera_settings->setDisabled(false);
            stream_button->setDisabled(false);
            record_button->setDisabled(false);

            xvc::change_camera_status(camera_id, "occupied");
            for (const auto &cap : camera["capabilities"]) {
                camera_settings->addItem(QString::fromStdString(cap.get<std::string>()));
            }
            setWindowTitle(QString::fromStdString(camera["name"].get<std::string>()));

            // for loop finds out a camera's status is idle
            // change it to occupied and break
            break;
            // }
        }
    }

    // window.setStyleSheet("QDockWidget:title {background-color: none;}");
    // window.setTitleBarWidget(new QWidget());
}

void StreamWidget::closeEvent(QCloseEvent *event)
{
    qInfo("StreamWidget::closeEvent");

    // stop_stream();
    if (streaming) {
        xvc::stop_stream(camera_id);
        if (pipeline) {
            set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }
    }
    xvc::change_camera_status(camera_id, "idle");
    safe_deque->clear();

    event->accept();
}

// void StreamWidget::play_stream(std::string url = "")
void StreamWidget::stream_handler()
{
    qInfo("StreamWidget::stream_handler");

    if (streaming) {
        qInfo("stop stream");

        stream_button->setText("Start");
        if (recording) {
            record_button->setDisabled(false);
            recording = false;
            xvc::stop_recording(GST_PIPELINE(pipeline));
        }
        stop_stream();
    } else {
        qInfo("start stream");

        stream_button->setText("Stop");
        start_stream();
    }
    streaming = !streaming;
}

void StreamWidget::start_stream()
{
    qInfo("StreamWidget::start_stream");

    pipeline = gst_pipeline_new(NULL);
    if (!pipeline) {
        g_error("Pipeline could be created");
        return;
    }

    port = xvc::port_pool->allocate_port();
    std::string ip = "192.168.50.241";
    std::string uri = ip + ":" + std::to_string(port);
    fmt::println("uri = {}", uri);

    xvc::open_video_stream(GST_PIPELINE(pipeline), uri);
    xvc::change_camera_status(camera_id, "playing");

    GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
    GstPad *parser_pad = gst_element_get_static_pad(parser, "sink");
    gst_pad_add_probe(
        parser_pad,
        GST_PAD_PROBE_TYPE_BUFFER,
        extract_timestamp_from_SEI,
        safe_deque,
        (GDestroyNotify) g_free
    );

    GstAppSinkCallbacks callbacks = {NULL};
    callbacks.new_sample = callback;
    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, NULL);

    set_state(pipeline, GST_STATE_PLAYING);

    QSettings settings;

    auto cap = settings.value("camera_cap").toString();
    // fmt::println("cap = {}", cap.toStdString());

    xvc::start_stream(camera_id, cap.toStdString(), port);
}

void StreamWidget::stop_stream()
{
    qInfo("StreamWidget::stop_stream");

    fmt::println("port = {}", port);
    xvc::port_pool->release_port(port);
    xvc::port_pool->print_available_ports();

    // if (streaming) {
    xvc::stop_stream(camera_id);
    xvc::change_camera_status(camera_id, "occupied");
    // }

    safe_deque->clear();
    if (pipeline) {
        set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
}

void StreamWidget::record_stream()
{
    qInfo("StreamWidget::record_stream");

    if (recording) {
        qInfo("stop recording");

        xvc::stop_recording(GST_PIPELINE(pipeline));
        stop_stream();
    } else {
        qInfo("start recording");

        if (!streaming) {
            start_stream();
        }

        record_button->setDisabled(true);
        stream_button->setText("Stop");
        streaming = true;

        QSettings settings;
        QString filepath = settings.value("record_directory", ".").toString();

        QString formatted_time = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        fmt::println("filepath = {}", formatted_time.toStdString());

        filepath += formatted_time;

        xvc::start_recording(GST_PIPELINE(pipeline), filepath.toStdString());
    }
    recording = !recording;
}

void StreamWidget::select_dir()
{
    qInfo("StreamWidget::select_dir");
    QSettings settings;
    QString filename = QFileDialog::getExistingDirectory(this, tr("Select Existing Directory"));
    settings.setValue("record_directory", filename);

    save_path->setText(filename);
    // QFontMetrics metrics(save_path->font());
    int text_width = save_path->fontMetrics().horizontalAdvance(save_path->text()) + 10;
    save_path->setFixedWidth(text_width);
}

void StreamWidget::select_camera_setting(QString camera_setting)
{
    qInfo("StreamWidget::select_camera_setting");
    QSettings settings;
    // fmt::println("{}", camera_setting.toStdString());
    settings.setValue("camera_cap", camera_setting);
}

void StreamWidget::parse_video()
{
    qInfo("StreamWidget::parse_video");
    QSettings settings;
    QString filepath =
        QFileDialog::getOpenFileName(this, tr("Select Video File"), "", tr("MKV Files (*.mkv)"));
    if (filepath.isEmpty()) return;

    pipeline = gst_pipeline_new("parse_video");
    if (!pipeline) {
        qDebug("Gstreamer pipeline could be created.");
        return;
    }

    xvc::parse_video(GST_PIPELINE(pipeline), filepath.toStdString());

    GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
    GstPad *parser_src_pad = gst_element_get_static_pad(parser, "src");
    gst_pad_add_probe(
        parser_src_pad,
        GST_PAD_PROBE_TYPE_BUFFER,
        extract_timestamp_from_SEI,
        safe_deque,
        (GDestroyNotify) g_free
    );

    GstAppSinkCallbacks callbacks = {NULL};
    callbacks.new_sample = callback;
    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, NULL);

    // g_object_set(G_OBJECT(sink), "emit-signals", TRUE, NULL);
    // g_signal_connect(sink, "new-sample", G_CALLBACK(callback), NULL);

    set_state(pipeline, GST_STATE_PLAYING);

    // gst_object_unref(pipeline);
}

void StreamWidget::display_frame(const QImage &image, const std::array<uint64_t, 4> timestamp)
{
    image_label->setPixmap(QPixmap::fromImage(image));

    timestamp_label->setText(QString::fromStdString(fmt::format(
        "{:08x}/{:08x}/{:08x}/{:08x}", timestamp[0], timestamp[1], timestamp[2], timestamp[3]
    )));
    // fmt::println("timestamp[0] = {:x}", timestamp[0]);
    // fmt::println("timestamp[1] = {:x}", timestamp[1]);
    // fmt::println("timestamp[2] = {:x}", timestamp[2]);
    // fmt::println("timestamp[3] = {:x}", timestamp[3]);
}