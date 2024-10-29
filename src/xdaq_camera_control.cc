#include "xdaq_camera_control.h"

#include <fmt/core.h>
#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <gst/gstpipeline.h>
#include <qnamespace.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <limits>
#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "camera_item_widget.h"
#include "record_settings.h"
#include "stream_window.h"


using nlohmann::json;


XDAQCameraControl::XDAQCameraControl()
    : QMainWindow(nullptr), stream_mainwindow(nullptr), record_settings(nullptr), elapsed_time(0)
{
    setMinimumSize(600, 300);
    resize(600, 300);
    stream_mainwindow = new StreamMainWindow(this);

    QWidget *central = new QWidget(this);
    QLabel *title = new QLabel(tr("XDAQ Camera Control"));
    QFont title_font("Arial", 15, QFont::Bold);
    title->setFont(title_font);

    QGridLayout *main_layout = new QGridLayout;
    QHBoxLayout *record_layout = new QHBoxLayout;
    record_button = new QPushButton(tr("REC"));

    timer = new QTimer(this);
    record_time = new QLabel(tr("00:00:00"));
    QFont record_time_font("Arial", 10);
    record_time->setFont(record_time_font);

    QPushButton *record_settings_button = new QPushButton(tr("SETTINGS"));

    cameras_list = new QListWidget(this);
    load_cameras();
    mock_camera();

    record_settings = new RecordSettings(cameras, nullptr);

    record_button->setFixedWidth(record_button->sizeHint().width());

    setWindowTitle(tr(" "));
    setCentralWidget(central);
    // setWindowFlags(Qt::CustomizeWindowHint);

    central->setLayout(main_layout);
    QWidget *record_widget = new QWidget(this);
    record_widget->setLayout(record_layout);
    record_layout->addWidget(record_button);
    record_layout->addWidget(record_time);

    main_layout->addWidget(record_widget, 1, 0, Qt::AlignLeft);
    main_layout->addWidget(title, 0, 0, Qt::AlignLeft);
    main_layout->addWidget(record_settings_button, 1, 2, Qt::AlignRight);
    main_layout->addWidget(cameras_list, 2, 0, 2, 3);

    bool recording = false;

    connect(timer, &QTimer::timeout, [this]() {
        ++elapsed_time;

        int hours = elapsed_time / 3600;
        int minutes = (elapsed_time % 3600) / 60;
        int seconds = elapsed_time % 60;

        record_time->setText(
            QString::fromStdString(fmt::format("{:02}:{:02}:{:02}", hours, minutes, seconds))
        );
    });
    connect(
        record_button,
        &QPushButton::clicked,
        [this, recording, record_settings_button]() mutable {
            if (!recording) {
                recording = true;
                elapsed_time = 0;
                record_time->setText(tr("00:00:00"));
                timer->start(1000);
                record_button->setText(tr("STOP"));

                QSettings settings("KonteX", "VC");
                auto save_path = settings.value("save_path", QDir::currentPath()).toString();
                QString dir_name;
                if (settings.value("dir_date", true).toBool()) {
                    dir_name = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
                    settings.setValue("dir_name", dir_name);
                } else {
                    dir_name = settings.value("dir_name").toString();
                }
                bool split_record = settings.value("split_record", false).toBool();
                int record_seconds = settings.value("record_seconds", 10).toInt();

                QDir dir(save_path);
                if (!dir.exists(dir_name)) {
                    if (!dir.mkdir(dir_name)) {
                        qDebug() << "Failed to create directory: " << save_path + "/" + dir_name;
                    }
                }
                QList<StreamWindow *> stream_windows =
                    stream_mainwindow->findChildren<StreamWindow *>();
                for (StreamWindow *window : stream_windows) {
                    fmt::println(
                        "save_path = {}, dir_name = {}",
                        save_path.toStdString(),
                        dir_name.toStdString()
                    );
                    std::string camera_name = window->camera->get_name();
                    std::string filepath =
                        save_path.toStdString() + "/" + dir_name.toStdString() + "/" + camera_name;

                    window->first_iframe_timestamp = std::numeric_limits<uint64_t>::max();
                    window->start_save_bin = false;
                    
                    window->saved_video_path = filepath + "-00.mkv";
                    xvc::start_recording(GST_PIPELINE(window->pipeline), filepath);

                    auto record_parser =
                        gst_bin_get_by_name(GST_BIN(window->pipeline), "record_parser");



                    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
                        gst_element_get_static_pad(record_parser, "sink"), gst_object_unref
                    );

                    // std::unique_ptr<GstPad, decltype(&gst_object_unref)> src_pad(
                    //     gst_element_get_static_pad(splitmuxsink, "src"), gst_object_unref
                    // );

                    // std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
                    //     gst_element_request_pad_simple(splitmuxsink, "video"), gst_object_unref
                    // );


                    if (QSettings("KonteX", "VC").value("additional_metadata", false).toBool()) {
                        window->open_filestream();
                    }
                    gst_pad_add_probe(
                        sink_pad.get(),
                        GST_PAD_PROBE_TYPE_BUFFER,
                        [](GstPad *pad, GstPadProbeInfo *info, gpointer user_data
                        ) -> GstPadProbeReturn {
                            fmt::print(
                                ">>>>>>>>>>>>>>>>>>>>>>>>>>>in "
                                "callback>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"
                            );
                            auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
                            auto pts = buffer->pts;
                            fmt::print(">>>>>>>>>>>>>>>>>>>>>>>>>>>recorded pts: {}\n", pts);

                            StreamWindow *window = (StreamWindow *) user_data;

                            auto metadata =
                                window->safe_deque_filesink->check_pts_pop_timestamp(pts);

                            if (metadata.has_value()) {
                                // fmt::print("pts: {}\n", pts);


                                fmt::print(
                                    ">>>>>>>>>>>>>>>>pts: {},  fpgatimestamp: {}, "
                                    "window->first_iframe_timestamp: {}\n",
                                    pts,
                                    metadata.value().fpga_timestamp,
                                    window->first_iframe_timestamp
                                );

                                window->record_parse_counter++;
                                // spdlog::info("write data");

                                if (pts >= window->first_iframe_timestamp &&
                                    window->start_save_bin) {
                                    fmt::print("!!!!!!saved as bin!!!!!!\n");
                                    // window->filestream.write((const char *) &pts, sizeof(pts));
                                    // window->filestream.write(
                                    //     (const char *) &(metadata.value()), sizeof(metadata.value())
                                    // );
                                    window->record_parse_save_counter++;
                                }
                            }
                            return GST_PAD_PROBE_OK;
                        },
                        window,
                        NULL
                    );
                    // gst_pad_add_probe(
                    //     src_pad.get(),
                    //     GST_PAD_PROBE_TYPE_BUFFER,
                    //     [](GstPad *pad, GstPadProbeInfo *info, gpointer user_data
                    //     ) -> GstPadProbeReturn {
                    //         fmt::print("<<<<<<<<<<<<<<<<<<<<<<in callback<<<<<<<<<<<<<<<<<<<<<<\n");
                    //         auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
                    //         auto pts = buffer->pts;
                    //         fmt::print("<<<<<<<<<<<<<<<<<<<<<<recorded pts: {}", pts);
                    //         return GST_PAD_PROBE_OK;
                    //     },
                    //     window,
                    //     NULL
                    // );
                    window->recording = true;

                    if (split_record) {
                        const unsigned int SEC = 1'000'000'000;
                        auto filesink = gst_bin_get_by_name(GST_BIN(window->pipeline), "filesink");
                        g_object_set(
                            G_OBJECT(filesink), "max-size-time", record_seconds * SEC, NULL
                        );  // in ns
                    } else {
                        auto filesink = gst_bin_get_by_name(GST_BIN(window->pipeline), "filesink");
                        g_object_set(G_OBJECT(filesink), "max-size-time", 0, NULL);  // continuous
                    }
                    // if (QSettings("KonteX", "VC").value("additional_metadata", false).toBool()) {
                    //     window->open_filestream();
                    // }
                }
                // TODO: disable for convenience
                cameras_list->setDisabled(true);
                record_settings_button->setDisabled(true);
                record_settings->hide();

            } else {
                recording = false;
                QList<StreamWindow *> stream_windows =
                    stream_mainwindow->findChildren<StreamWindow *>();
                for (StreamWindow *window : stream_windows) {
                    xvc::stop_recording(GST_PIPELINE(window->pipeline));
                    window->recording = false;
                    window->close_filestream();


                    // post-process to generate binary
                    xvc::parse_video_save_binary(window->saved_video_path);
                }
                record_button->setText(tr("REC"));
                timer->stop();
                // TODO: stop record, button is clickable again.
                cameras_list->setDisabled(false);
                record_settings_button->setDisabled(false);
            }
        }
    );
    connect(record_settings_button, &QPushButton::clicked, [this]() { record_settings->show(); });
}

void XDAQCameraControl::load_cameras()
{
    std::string cameras_str = Camera::list_cameras("192.168.177.100:8000/cameras");
    if (!cameras_str.empty()) {
        json cameras_json = json::parse(cameras_str);
        for (const auto &camera_json : cameras_json) {
            Camera *camera = new Camera(camera_json["id"], camera_json["name"]);
            for (const auto &capability : camera_json["capabilities"]) {
                Camera::Cap cap;

                cap.media_type = capability.at("media_type").get<std::string>();
                cap.format = capability.contains("format")
                                 ? capability.at("format").get<std::string>()
                                 : "N/A";
                // FIXME: modify json return width and height int
                cap.width = std::stoi(capability.at("width").get<std::string>());
                cap.height = std::stoi(capability.at("height").get<std::string>());
                std::string framerate_str = capability.at("framerate").get<std::string>();

                size_t delimiter_pos = framerate_str.find('/');
                if (delimiter_pos != std::string::npos) {
                    cap.fps_n = std::stoi(framerate_str.substr(0, delimiter_pos));
                    cap.fps_d = std::stoi(framerate_str.substr(delimiter_pos + 1));
                }
                camera->add_cap(cap);
            }
            QListWidgetItem *item = new QListWidgetItem(cameras_list);
            CameraItemWidget *camera_item_widget = new CameraItemWidget(camera, this);
            item->setSizeHint(camera_item_widget->sizeHint());
            cameras_list->setItemWidget(item, camera_item_widget);
            cameras.emplace_back(camera);
        }
    }
}

void XDAQCameraControl::mock_camera()
{
    Camera *camera = new Camera(-1, "[TEST] HFR");
    Camera::Cap cap;
    cap.media_type = "image/jpeg";
    cap.format = "UYVY";
    cap.width = 720;
    cap.height = 540;
    cap.fps_n = 60;
    cap.fps_d = 1;
    camera->add_cap(cap);
    QListWidgetItem *item = new QListWidgetItem(cameras_list);
    CameraItemWidget *camera_item_widget = new CameraItemWidget(camera, this);
    item->setSizeHint(camera_item_widget->sizeHint());
    cameras_list->setItemWidget(item, camera_item_widget);
    cameras.emplace_back(camera);
}

void XDAQCameraControl::mousePressEvent(QMouseEvent *e)
{
    if (record_settings && !record_settings->geometry().contains(e->pos())) {
        record_settings->close();
    }
}

void XDAQCameraControl::closeEvent(QCloseEvent *e)
{
    for (auto camera : cameras) {
        camera->stop();
    }
    record_settings->close();
    e->accept();
}