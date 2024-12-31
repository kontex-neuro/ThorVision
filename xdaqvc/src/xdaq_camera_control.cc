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
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <future>
#include <nlohmann/json.hpp>
#include <thread>

#include "camera_item_widget.h"
#include "record_settings.h"
#include "xdaqvc/xvc.h"

using nlohmann::json;


namespace
{
auto constexpr CONTINUOUS = "continuous";
auto constexpr MAX_SIZE_TIME = "max_size_time";
auto constexpr MAX_FILES = "max_files";

auto constexpr SAVE_PATHS = "save_paths";
auto constexpr DIR_DATE = "dir_date";
auto constexpr DIR_NAME = "dir_name";

auto constexpr EVENT_TYPE = "event_type";
auto constexpr ID = "id";
auto constexpr NAME = "name";
auto constexpr CAMERA = "camera";
auto constexpr CAPS = "caps";
auto constexpr MEDIA_TYPE = "media_type";
auto constexpr FORMAT = "format";
auto constexpr WIDTH = "width";
auto constexpr HEIGHT = "height";
auto constexpr FRAMERATE = "framerate";
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

    setWindowTitle(" ");
    setWindowIcon(QIcon());
    setCentralWidget(central);

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

    _ws_client =
        std::make_unique<xvc::ws_client>("192.168.177.100", "8000", [this](const std::string &msg) {
            auto const device_event = json::parse(msg);
            auto const event_type = device_event[EVENT_TYPE];
            auto const camera_json = device_event[CAMERA];
            auto const id = camera_json[ID].get<int>();
            auto const name = camera_json[NAME].get<std::string>();
            auto const caps_json = camera_json[CAPS];

            // auto camera = std::make_shared<Camera>(id, name);
            auto camera = new Camera(id, name);

            for (auto const &cap_json : caps_json) {
                Camera::Cap cap;

                cap.media_type = cap_json[MEDIA_TYPE].get<std::string>();
                cap.format = cap_json[FORMAT].get<std::string>();
                cap.width = cap_json[WIDTH].get<int>();
                cap.height = cap_json[HEIGHT].get<int>();
                auto framerate_str = cap_json[FRAMERATE].get<std::string>();

                auto delimiter_pos = framerate_str.find('/');
                if (delimiter_pos != std::string::npos) {
                    cap.fps_n = std::stoi(framerate_str.substr(0, delimiter_pos));
                    cap.fps_d = std::stoi(framerate_str.substr(delimiter_pos + 1));
                }
                camera->add_cap(cap);
            }

            QMetaObject::invokeMethod(
                this,
                [this, event_type, camera, name, id]() {
                    if (event_type == "Added") {
                        auto item = new QListWidgetItem(cameras_list);
                        auto widget = new CameraItemWidget(camera, this);
                        item->setData(Qt::UserRole, id);
                        item->setSizeHint(widget->sizeHint());
                        cameras_list->setItemWidget(item, widget);
                        cameras.emplace_back(camera);
                    } else if (event_type == "Removed") {
                        for (int i = 0; i < cameras_list->count(); ++i) {
                            auto item = cameras_list->item(i);
                            if (item->data(Qt::UserRole).toInt() == id) {
                                delete cameras_list->takeItem(i);
                                cameras.erase(
                                    std::remove_if(
                                        cameras.begin(),
                                        cameras.end(),
                                        [id](auto const &camera) { return camera->id() == id; }
                                    ),
                                    cameras.end()
                                );
                                break;
                            }
                        }
                    }
                },
                Qt::QueuedConnection
            );
        });

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

            QSettings settings("KonteX", "ThorVision");
            auto continuous = settings.value(CONTINUOUS, true).toBool();
            auto max_size_time = settings.value(MAX_SIZE_TIME, 10).toInt();
            auto max_files = settings.value(MAX_FILES, 10).toInt();

            auto save_path = settings.value(SAVE_PATHS, QDir::currentPath()).toStringList().first();
            auto dir_name = settings.value(DIR_DATE, true).toBool()
                                ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss")
                                : settings.value(DIR_NAME).toString();

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
                                window->camera->name();
                window->saved_video_path = filepath.string() + "-00.mkv";
                // gstreamer uses '/' as the path separator
                for (auto &c : window->saved_video_path) {
                    if (c == '\\') {
                        c = '/';
                    }
                }

                if (window->camera->current_cap().find("image/jpeg") != std::string::npos) {
                    xvc::start_jpeg_recording(
                        GST_PIPELINE(window->pipeline),
                        filepath,
                        continuous,
                        max_size_time,
                        max_files
                    );
                } else {
                    window->start_h265_recording(filepath, continuous, max_size_time, max_files);
                }
            }
            cameras_list->setDisabled(true);
            record_settings->hide();

        } else {
            recording = false;
            for (auto window : stream_mainwindow->findChildren<StreamWindow *>()) {
                if (window->camera->current_cap().find("image/jpeg") != std::string::npos) {
                    xvc::stop_jpeg_recording(GST_PIPELINE(window->pipeline));
                    // Create promise/future pair to track completion
                    std::promise<void> promise;
                    std::future<void> future = promise.get_future();

                    parsing_threads.emplace_back(
                        std::thread([window, promise = std::move(promise)]() mutable {
                            xvc::parse_video_save_binary_jpeg(window->saved_video_path);
                            promise.set_value();  // Signal completion
                        }),
                        std::move(future)
                    );
                } else {
                    xvc::stop_h265_recording(GST_PIPELINE(window->pipeline));
                    // Create promise/future pair to track completion
                    std::promise<void> promise;
                    std::future<void> future = promise.get_future();

                    parsing_threads.emplace_back(
                        std::thread([window, promise = std::move(promise)]() mutable {
                            xvc::parse_video_save_binary_h265(window->saved_video_path);
                            promise.set_value();  // Signal completion
                        }),
                        std::move(future)
                    );
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
    auto const cameras_str = Camera::cameras();
    if (!cameras_str.empty()) {
        auto const cameras_json = json::parse(cameras_str);

        for (auto const &camera_json : cameras_json) {
            auto const id = camera_json[ID].get<int>();
            auto const name = camera_json[NAME].get<std::string>();
            auto const caps_json = camera_json[CAPS];

            auto camera = new Camera(id, name);

            for (auto const &cap_json : caps_json) {
                Camera::Cap cap;

                cap.media_type = cap_json[MEDIA_TYPE].get<std::string>();
                cap.format = cap_json[FORMAT].get<std::string>();
                cap.width = cap_json[WIDTH].get<int>();
                cap.height = cap_json[HEIGHT].get<int>();

                auto framerate_str = cap_json[FRAMERATE].get<std::string>();
                auto delimiter_pos = framerate_str.find('/');
                if (delimiter_pos != std::string::npos) {
                    cap.fps_n = std::stoi(framerate_str.substr(0, delimiter_pos));
                    cap.fps_d = std::stoi(framerate_str.substr(delimiter_pos + 1));
                }
                camera->add_cap(cap);
            }
            auto item = new QListWidgetItem(cameras_list);
            auto widget = new CameraItemWidget(camera, this);
            item->setData(Qt::UserRole, id);
            item->setSizeHint(widget->sizeHint());
            cameras_list->setItemWidget(item, widget);
            cameras.emplace_back(camera);
        }
    }
}

void XDAQCameraControl::mock_camera()
{
    auto camera = new Camera(-1, "[TEST] videotestsrc");
    Camera::Cap cap;
    cap.media_type = "video/x-raw";
    cap.format = "UYVY";
    cap.width = 720;
    cap.height = 540;
    cap.fps_n = 60;
    cap.fps_d = 1;
    camera->add_cap(cap);
    auto item = new QListWidgetItem(cameras_list);
    auto widget = new CameraItemWidget(camera, this);
    item->setSizeHint(widget->sizeHint());
    cameras_list->setItemWidget(item, widget);
    cameras.emplace_back(camera);
}

void XDAQCameraControl::mousePressEvent(QMouseEvent *e)
{
    if (record_settings && !record_settings->geometry().contains(e->pos())) {
        record_settings->close();
    }
}

bool XDAQCameraControl::are_threads_finished() const
{
    auto *self = const_cast<XDAQCameraControl *>(this);
    self->cleanup_finished_threads();
    printf("parsing_threads.size() = %zu\n", parsing_threads.size());
    return parsing_threads.empty();
}

void XDAQCameraControl::wait_for_threads()
{
    cleanup_finished_threads();
    for (auto &thread : parsing_threads) {
        if (thread.first.joinable()) {
            thread.first.join();
        }
    }
    parsing_threads.clear();
}

void XDAQCameraControl::closeEvent(QCloseEvent *e)
{
    if (!are_threads_finished()) {
        auto reply = QMessageBox::warning(
            this,
            tr("Warning"),
            tr("Video parsing is still in progress. Do you want to wait for completion?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
        );

        if (reply == QMessageBox::Yes) {
            wait_for_threads();
            for (auto camera : cameras) {
                camera->stop();
            }
            record_settings->close();
            e->accept();
        } else if (reply == QMessageBox::No) {
            // Force close, threads will be terminated
            for (auto &thread : parsing_threads) {
                if (thread.first.joinable()) {
                    thread.first.detach();
                }
            }
            for (auto camera : cameras) {
                camera->stop();
            }
            record_settings->close();
            e->accept();
        } else {
            e->ignore();
        }
    } else {
        for (auto camera : cameras) {
            camera->stop();
        }
        // _ws_client->stop();
        record_settings->close();
        e->accept();
    }
}

void XDAQCameraControl::cleanup_finished_threads()
{
    parsing_threads.erase(
        std::remove_if(
            parsing_threads.begin(),
            parsing_threads.end(),
            [](auto &thread_future) {
                auto &[thread, future] = thread_future;
                // Check if thread is done using future
                if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    thread.join();  // Clean up the thread
                    return true;    // Remove from vector
                }
                return false;  // Keep in vector
            }
        ),
        parsing_threads.end()
    );
}