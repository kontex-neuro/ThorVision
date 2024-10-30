#include "xdaq_camera_control.h"

#include <fmt/core.h>
#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <gst/gstpipeline.h>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

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
#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "camera_item_widget.h"
#include "record_settings.h"


using nlohmann::json;


namespace
{
constexpr auto SAVE_PATHS = "save_paths";
constexpr auto DIR_DATE = "dir_date";
constexpr auto DIR_NAME = "dir_name";
constexpr auto SPLIT_RECORD = "split_record";
constexpr auto RECORD_SECONDS = "record_seconds";
constexpr auto SEC = 1'000'000'000;
}  // namespace


XDAQCameraControl::XDAQCameraControl()
    : QMainWindow(nullptr), stream_mainwindow(nullptr), record_settings(nullptr), elapsed_time(0)
{
    setMinimumSize(600, 300);
    resize(600, 300);

    stream_mainwindow = new StreamMainWindow(this);
    auto central = new QWidget(this);
    auto main_layout = new QGridLayout;
    auto record_layout = new QHBoxLayout;
    auto title = new QLabel(tr("XDAQ Camera Control"));
    QFont title_font;
    title_font.setPointSize(15);
    title_font.setBold(true);
    title->setFont(title_font);

    setWindowTitle(tr(" "));
    setCentralWidget(central);
    // setWindowFlags(Qt::CustomizeWindowHint);

    record_button = new QPushButton(tr("REC"));
    record_button->setFixedWidth(record_button->sizeHint().width());
    timer = new QTimer(this);
    record_time = new QLabel(tr("00:00:00"));
    QFont record_time_font;
    record_time_font.setPointSize(10);
    record_time->setFont(record_time_font);

    // TODO: new RecordSettings must be after load_camera()
    // because the constructor of RecordSettings reads camera info
    auto record_settings_button = new QPushButton(tr("SETTINGS"));
    cameras_list = new QListWidget(this);
    load_cameras();
    // mock_camera();
    record_settings = new RecordSettings(cameras, nullptr);

    auto record_widget = new QWidget(this);
    central->setLayout(main_layout);
    record_widget->setLayout(record_layout);
    record_layout->addWidget(record_button);
    record_layout->addWidget(record_time);

    main_layout->addWidget(record_widget, 1, 0, Qt::AlignLeft);
    main_layout->addWidget(title, 0, 0, Qt::AlignLeft);
    main_layout->addWidget(record_settings_button, 1, 2, Qt::AlignRight);
    main_layout->addWidget(cameras_list, 2, 0, 2, 3);

    auto recording = false;

    connect(timer, &QTimer::timeout, [this]() {
        ++elapsed_time;

        auto hours = elapsed_time / 3600;
        auto minutes = (elapsed_time % 3600) / 60;
        auto seconds = elapsed_time % 60;

        record_time->setText(
            QString::fromStdString(fmt::format("{:02}:{:02}:{:02}", hours, minutes, seconds))
        );
    });
    connect(record_button, &QPushButton::clicked, [this, recording]() mutable {
        if (!recording) {
            recording = true;
            elapsed_time = 0;
            record_time->setText(tr("00:00:00"));
            timer->start(1000);
            record_button->setText(tr("STOP"));

            QSettings settings("KonteX", "VC");
            auto save_path = settings.value(SAVE_PATHS, QDir::currentPath()).toStringList().first();
            auto dir_name = settings.value(DIR_DATE, true).toBool()
                                ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss")
                                : settings.value(DIR_NAME).toString();
            auto split_record = settings.value(SPLIT_RECORD, false).toBool();
            auto record_seconds = settings.value(RECORD_SECONDS, 10).toInt();

            // TODO: Duplicate the directory creation code from stream_window.cc here.
            // This is for the record button press,
            // whereas the code in stream_window.cc is used in the callback for a DDS trigger.
            auto path = fs::path(save_path.toStdString()) / dir_name.toStdString();
            if (!fs::exists(path)) {
                spdlog::info("create_directory = {}", path.generic_string());
                std::error_code ec;
                if (!fs::create_directories(path, ec)) {
                    spdlog::info(
                        "Failed to create directory: {}. Error: {}",
                        path.generic_string(),
                        ec.message()
                    );
                }
            }

            for (auto window : stream_mainwindow->findChildren<StreamWindow *>()) {
                auto filepath = fs::path(save_path.toStdString()) / dir_name.toStdString() /
                                window->camera->get_name();
                window->saved_video_path = filepath.string() + "-00.mkv";
                // gstreamer uses '/' as the path separator
                for (auto &c : window->saved_video_path) {
                    if (c == '\\') {
                        c = '/';
                    }
                }

                if (window->camera->get_current_cap().find("image/jpeg") != std::string::npos) {
                    xvc::start_jpeg_recording(GST_PIPELINE(window->pipeline), filepath);
                } else {
                    xvc::start_h265_recording(GST_PIPELINE(window->pipeline), filepath);
                }

                spdlog::info(
                    "split_record = {}, record_seconds = {}", split_record, record_seconds
                );
                auto filesink = gst_bin_get_by_name(GST_BIN(window->pipeline), "filesink");
                g_object_set(
                    G_OBJECT(filesink),
                    "max-size-time",
                    split_record ? record_seconds * SEC : 0,
                    nullptr
                );
            }
            cameras_list->setDisabled(true);
            record_settings->hide();

        } else {
            recording = false;
            for (auto window : stream_mainwindow->findChildren<StreamWindow *>()) {
                if (window->camera->get_current_cap().find("image/jpeg") != std::string::npos) {
                    xvc::stop_jpeg_recording(GST_PIPELINE(window->pipeline));
                } else {
                    xvc::stop_h265_recording(GST_PIPELINE(window->pipeline));
                    xvc::parse_video_save_binary(window->saved_video_path);
                }
            }
            record_button->setText(tr("REC"));
            timer->stop();

            // TODO: stop record, button is clickable again.
            cameras_list->setDisabled(false);
        }
    });
    connect(record_settings_button, &QPushButton::clicked, [this]() { record_settings->show(); });
}

void XDAQCameraControl::load_cameras()
{
    auto cameras_str = Camera::list_cameras("192.168.177.100:8000/cameras");
    if (!cameras_str.empty()) {
        auto cameras_json = json::parse(cameras_str);
        for (const auto &camera_json : cameras_json) {
            auto camera = new Camera(camera_json["id"], camera_json["name"]);
            for (const auto &capability : camera_json["capabilities"]) {
                Camera::Cap cap;

                cap.media_type = capability.at("media_type").get<std::string>();
                cap.format = capability.contains("format")
                                 ? capability.at("format").get<std::string>()
                                 : "N/A";
                // FIXME: modify json return width and height int
                cap.width = std::stoi(capability.at("width").get<std::string>());
                cap.height = std::stoi(capability.at("height").get<std::string>());
                auto framerate_str = capability.at("framerate").get<std::string>();

                auto delimiter_pos = framerate_str.find('/');
                if (delimiter_pos != std::string::npos) {
                    cap.fps_n = std::stoi(framerate_str.substr(0, delimiter_pos));
                    cap.fps_d = std::stoi(framerate_str.substr(delimiter_pos + 1));
                }
                camera->add_cap(cap);
            }
            auto item = new QListWidgetItem(cameras_list);
            auto camera_item_widget = new CameraItemWidget(camera, this);
            item->setSizeHint(camera_item_widget->sizeHint());
            cameras_list->setItemWidget(item, camera_item_widget);
            cameras.emplace_back(camera);
        }
    }
}

void XDAQCameraControl::mock_camera()
{
    auto camera = new Camera(-1, "[TEST] HFR");
    Camera::Cap cap;
    cap.media_type = "image/jpeg";
    cap.format = "UYVY";
    cap.width = 720;
    cap.height = 540;
    cap.fps_n = 60;
    cap.fps_d = 1;
    camera->add_cap(cap);
    auto item = new QListWidgetItem(cameras_list);
    auto camera_item_widget = new CameraItemWidget(camera, this);
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