#include "xdaq_camera_control.h"

#include <fmt/core.h>
#include <gst/gstpipeline.h>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <future>
#include <nlohmann/json.hpp>
#include <thread>

#include "camera_item_widget.h"
#include "record_confirm_dialog.h"
#include "record_settings.h"
#include "server_status_indicator.h"
#include "stream_window.h"
#include "xdaqvc/xvc.h"

using nlohmann::json;

namespace
{
auto constexpr CONTINUOUS = "continuous";
auto constexpr MAX_SIZE_TIME = "max_size_time";
auto constexpr TIME_UNIT = "time_unit";
auto constexpr MAX_FILES = "max_files";

auto constexpr SAVE_PATHS = "save_paths";
auto constexpr DIR_DATE = "dir_date";
auto constexpr DIR_NAME = "dir_name";

auto constexpr OPEN_VIDEO_FOLDER = "open_video_folder";

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

auto constexpr VIDEO_MJPEG = "image/jpeg";
auto constexpr VIDEO_RAW = "video/x-raw";

xvc::TimeUnit to_time_unit(int index)
{
    switch (index) {
    case 0: return xvc::TimeUnit::Seconds;
    case 1: return xvc::TimeUnit::Minutes;
    case 2: return xvc::TimeUnit::Hours;
    case 3: return xvc::TimeUnit::Days;
    default:
        spdlog::warn("Invalid time unit {}, defaulting to Seconds", index);
        return xvc::TimeUnit::Seconds;
    }
}

Camera *parse_and_find(const json &camera_json, std::vector<Camera *> &cameras)
{
    auto const id = camera_json[ID].get<int>();
    auto it = std::find_if(cameras.begin(), cameras.end(), [id](Camera *camera) {
        return camera->id() == id;
    });
    if (it != cameras.end()) {
        return *it;
    }

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
    return camera;
}
}  // namespace


XDAQCameraControl::XDAQCameraControl()
    : QMainWindow(nullptr),
      _stream_mainwindow(nullptr),
      _record_settings(nullptr),
      _elapsed_time(0),
      _recording(false),
      _skip_dialog(false)
{
    spdlog::info("Creating XDAQCameraControl");
    _stream_mainwindow = new StreamMainWindow();
    auto central = new QWidget(this);
    auto main_layout = new QGridLayout(central);
    auto title = new QLabel(tr("XDAQ Camera Control"), this);
    QFont title_font;
    title_font.setPointSize(15);
    title_font.setBold(true);
    title->setFont(title_font);

    resize(600, 300);
    setCentralWidget(central);

    _record_button = new QPushButton(tr("REC"), this);
    _record_button->setFixedWidth(_record_button->sizeHint().width());
    _record_button->setEnabled(false);
    _timer = new QTimer(this);
    _record_time = new QLabel(tr("00:00:00"), this);
    QFont record_time_font;
    record_time_font.setPointSize(10);
    _record_time->setFont(record_time_font);

    auto settings_button = new QPushButton(tr("SETTINGS"), this);
    settings_button->setFixedWidth(settings_button->sizeHint().width());

    _record_settings = new RecordSettings();

    _camera_list = new QListWidget(this);

    auto server_status_indicator = new ServerStatusIndicator(this);

    auto record_widget = new QWidget(this);
    auto record_layout = new QHBoxLayout(record_widget);
    record_layout->addWidget(_record_button);
    record_layout->addWidget(_record_time);

    main_layout->addWidget(title, 0, 0, Qt::AlignLeft);
    main_layout->addWidget(server_status_indicator, 0, 2, Qt::AlignRight);
    main_layout->addWidget(record_widget, 1, 0, Qt::AlignLeft);
    main_layout->addWidget(settings_button, 1, 2, Qt::AlignRight);
    main_layout->addWidget(_camera_list, 2, 0, 2, 3);

    _ws_client = std::make_unique<xvc::ws_client>([this](const std::string &event) {
        auto const device_event = json::parse(event);
        auto const event_type = device_event[EVENT_TYPE];
        auto const camera_json = device_event[CAMERA];

        auto camera = parse_and_find(camera_json, _cameras);

        QMetaObject::invokeMethod(
            this,
            [this, event_type, camera]() {
                if (event_type == "Added") {
                    add_camera(camera);
                } else if (event_type == "Removed") {
                    remove_camera(camera->id());
                }
            },
            Qt::QueuedConnection
        );
    });

    connect(
        server_status_indicator,
        &ServerStatusIndicator::status_change,
        this,
        [this](bool is_server_on) {
            if (is_server_on) {
                auto const cameras_str = Camera::cameras();
                if (!cameras_str.empty()) {
                    auto const cameras_json = json::parse(cameras_str);

                    for (auto const &camera_json : cameras_json) {
                        auto camera = parse_and_find(camera_json, _cameras);
                        add_camera(camera);
                    }
                }
            } else {
                if (_recording) {
                    spdlog::info("Stop recording due to server is off.");
                    stop_record();
                }
                while (_cameras.size() > 0) {
                    auto camera = _cameras.front();
                    remove_camera(camera->id());
                }
            }
        }
    );
    connect(_timer, &QTimer::timeout, [this]() {
        ++_elapsed_time;

        auto hours = _elapsed_time / 3600;
        auto minutes = (_elapsed_time % 3600) / 60;
        auto seconds = _elapsed_time % 60;

        _record_time->setText(
            QString::fromStdString(fmt::format("{:02}:{:02}:{:02}", hours, minutes, seconds))
        );
    });
    connect(_record_button, &QPushButton::clicked, [this]() mutable {
        if (!_skip_dialog && !_recording) {
            auto specs = QString::fromStdString("");
            for (auto [_, item] : _camera_item_map) {
                auto widget = qobject_cast<CameraItemWidget *>(_camera_list->itemWidget(item));
                auto cap = widget->cap();
                if (!cap.isEmpty()) {
                    specs.append(cap + "<br>");
                }
            }

            RecordConfirmDialog dialog(specs);
            if (dialog.exec() == QMessageBox::Accepted) {
                _skip_dialog = dialog.dont_ask_again() ? true : false;
                start_record();
            }
        } else {
            stop_record();
        }
    });
    connect(settings_button, &QPushButton::clicked, [this]() {
        _record_settings->show();
        _record_settings->raise();
    });
}

void XDAQCameraControl::start_record()
{
    _recording = true;
    _elapsed_time = 0;
    _record_time->setText(tr("00:00:00"));
    _timer->start(1000);
    _record_button->setText(tr("STOP"));
    _camera_list->setDisabled(true);

    QSettings settings("KonteX Neuroscience", "ThorVision");
    auto continuous = settings.value(CONTINUOUS, true).toBool();
    auto max_size_time = settings.value(MAX_SIZE_TIME, 10).toInt();
    auto time_unit = to_time_unit(settings.value(TIME_UNIT, 0).toInt());
    auto max_files = settings.value(MAX_FILES, 10).toInt();
    auto save_path =
        settings
            .value(SAVE_PATHS, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .toStringList()
            .first();
    auto dir_name = settings.value(DIR_DATE, true).toBool()
                        ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss")
                        : settings.value(DIR_NAME).toString();
    _start_record_dir_path = fs::path(save_path.toStdString()) / dir_name.toStdString();

    // TODO: Duplicate the directory creation code from stream_window.cc here.
    // This is for the record button press,
    // whereas the code in stream_window.cc is used in the callback for a DDS trigger.
    if (!fs::exists(_start_record_dir_path)) {
        spdlog::info("Create Directory = {}", _start_record_dir_path.generic_string());
        std::error_code ec;
        if (!fs::create_directories(_start_record_dir_path, ec)) {
            spdlog::info(
                "Failed to create directory: {}. Error: {}",
                _start_record_dir_path.generic_string(),
                ec.message()
            );
        }
    }

    for (auto window : _stream_mainwindow->findChildren<StreamWindow *>()) {
        auto filepath =
            _start_record_dir_path /
            fmt::format("{}-{}", window->windowTitle().toStdString(), window->_camera->id());

        if (window->_camera->current_cap().find(VIDEO_MJPEG) != std::string::npos ||
            window->_camera->current_cap().find(VIDEO_RAW) != std::string::npos) {
            xvc::start_jpeg_recording(
                GST_PIPELINE(window->_pipeline.get()),
                filepath,
                continuous,
                max_size_time,
                time_unit,
                max_files
            );
        } else {
            // TODO: disable h265 for now
            window->start_h265_recording(filepath, continuous, max_size_time, max_files);
        }
    }
}

void XDAQCameraControl::stop_record()
{
    _recording = false;
    _record_button->setEnabled(false);
    _record_button->setText(tr("REC"));
    _timer->stop();
    _camera_list->setDisabled(false);

    auto open_video_folder =
        QSettings("KonteX Neuroscience", "ThorVision").value(OPEN_VIDEO_FOLDER, true).toBool();

    for (auto window : _stream_mainwindow->findChildren<StreamWindow *>()) {
        if (window->_camera->current_cap().find(VIDEO_MJPEG) != std::string::npos ||
            window->_camera->current_cap().find(VIDEO_RAW) != std::string::npos) {
            xvc::stop_jpeg_recording(GST_PIPELINE(window->_pipeline.get()));

            // Create promise/future pair to track completion
            std::promise<void> promise;
            std::future<void> future = promise.get_future();

            _gstreamer_handler_threads.emplace_back(
                std::thread([window, promise = std::move(promise)]() mutable {
                    xvc::stop_jpeg_recording(GST_PIPELINE(window->_pipeline.get()));
                    promise.set_value();
                }),
                std::move(future)
            );

            if (open_video_folder) {
                auto directory =
                    QFileInfo(QString::fromStdString(_start_record_dir_path.generic_string()))
                        .filePath();
                QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
            }

        } else {
            // TODO: disable h265 for now
            xvc::stop_h265_recording(GST_PIPELINE(window->_pipeline.get()));
        }
    }
    QTimer::singleShot(3500, [this]() { _record_button->setEnabled(true); });
}

bool XDAQCameraControl::are_threads_finished() const
{
    auto *self = const_cast<XDAQCameraControl *>(this);
    self->cleanup_finished_threads();
    return _gstreamer_handler_threads.empty();
}

void XDAQCameraControl::wait_for_threads()
{
    cleanup_finished_threads();
    for (auto &thread : _gstreamer_handler_threads) {
        if (thread.first.joinable()) {
            thread.first.join();
        }
    }
    _gstreamer_handler_threads.clear();
}

void XDAQCameraControl::closeEvent(QCloseEvent *e)
{
    spdlog::info("Closing XDAQCameraControl");

    if (!are_threads_finished()) {
        auto reply = QMessageBox::warning(
            this,
            tr("Warning"),
            tr("Video parsing is still in progress. Do you want to wait for completion?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
        );

        if (reply == QMessageBox::Yes) {
            wait_for_threads();
            for (auto camera : _cameras) {
                camera->stop();
            }
            _record_settings->close();
            _stream_mainwindow->close();
            e->accept();
        } else if (reply == QMessageBox::No) {
            // Force close, threads will be terminated
            for (auto &thread : _gstreamer_handler_threads) {
                if (thread.first.joinable()) {
                    thread.first.detach();
                }
            }
            for (auto camera : _cameras) {
                camera->stop();
            }
            _record_settings->close();
            _stream_mainwindow->close();
            e->accept();
        } else {
            e->ignore();
        }
    } else {
        if (_recording) {
            auto reply = QMessageBox::warning(
                this,
                "Recording in Progress",
                "A recording is still in progress. Do you really want to exit?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                e->ignore();
                return;
            }
            stop_record();
        }

        _timer->stop();

        for (auto &[_, window] : _camera_window_map) {
            window->close();
        }
        _camera_window_map.clear();

        for (auto camera : _cameras) {
            camera->stop();
            delete camera;
        }
        _cameras.clear();

        _record_settings->close();
        _stream_mainwindow->close();

        e->accept();
    }
}

void XDAQCameraControl::cleanup_finished_threads()
{
    _gstreamer_handler_threads.erase(
        std::remove_if(
            _gstreamer_handler_threads.begin(),
            _gstreamer_handler_threads.end(),
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
        _gstreamer_handler_threads.end()
    );
}

void XDAQCameraControl::add_camera(Camera *camera)
{
    auto id = camera->id();
    auto item = new QListWidgetItem(_camera_list);
    auto widget = new CameraItemWidget(camera, _camera_list);

    item->setData(Qt::UserRole, id);
    item->setSizeHint(widget->sizeHint());

    _camera_list->setItemWidget(item, widget);
    _cameras.emplace_back(camera);
    _camera_item_map[id] = item;

    _record_settings->add_camera(camera);

    connect(widget, &CameraItemWidget::stream_toggle, [this](Camera *camera, bool checked) {
        auto id = camera->id();
        auto widget =
            qobject_cast<CameraItemWidget *>(_camera_list->itemWidget(_camera_item_map[id]));
        if (!widget) return;

        if (checked) {
            auto stream_window = new StreamWindow(camera, widget->view(), _stream_mainwindow);
            _camera_window_map[id] = stream_window;

            connect(
                stream_window,
                &StreamWindow::window_close,
                this,
                [this, id, stream_window, widget]() {
                    _camera_window_map.erase(id);
                    widget->_stream->setChecked(false);
                    stream_window->deleteLater();

                    if (_camera_window_map.empty()) {
                        _stream_mainwindow->close();
                    } else {
                        _stream_mainwindow->adjustSize();
                    }
                    _record_button->setEnabled(!_camera_window_map.empty());
                }
            );
            connect(
                widget->_name, &NameLabel::name_changed, stream_window, &StreamWindow::update_title
            );

            _stream_mainwindow->show();
            _stream_mainwindow->addDockWidget(Qt::TopDockWidgetArea, stream_window);

            auto windows = _stream_mainwindow->findChildren<StreamWindow *>();
            auto count = static_cast<int>(windows.size());

            if (count == 3) {
                _stream_mainwindow->splitDockWidget(windows[0], windows[2], Qt::Vertical);
            } else if (count == 4) {
                _stream_mainwindow->splitDockWidget(windows[1], windows[3], Qt::Vertical);
            }
            stream_window->play();

        } else {
            spdlog::info("Stop camera stream for camera id: {}, name: {}", id, camera->name());
            auto it = _camera_window_map.find(id);
            if (it != _camera_window_map.end()) {
                it->second->close();
            }
        }
        _record_button->setEnabled(!_camera_window_map.empty());
    });
    connect(widget, &CameraItemWidget::view_toggle, [this](Camera *camera, bool checked) {
        auto id = camera->id();
        auto stream_window = _camera_window_map[id];
        if (!stream_window) {
            spdlog::warn("Stream window not found for camera id: {}", id);
            return;
        }

        spdlog::info("Camera id: '{}', '{}' view toggled: {}", id, camera->name(), checked);
        checked ? stream_window->show() : stream_window->hide();
        xvc::decode_toggle(GST_PIPELINE(stream_window->_pipeline.get()), checked);
    });

    QTimer::singleShot(0, this, [this, widget, camera]() {
        auto record_widget = _record_settings->_camera_widget_map[camera->id()];
        if (!record_widget) {
            spdlog::warn("CameraRecordWidget not found for camera id: {}", camera->id());
            return;
        }
        connect(
            widget->_name,
            &NameLabel::name_changed,
            record_widget,
            &CameraRecordWidget::update_camera_name
        );
    });
};

void XDAQCameraControl::remove_camera(int const id)
{
    if (_recording) {
        stop_record();
        spdlog::warn("Recording Remove camera: {}", id);

        QMessageBox::warning(
            this,
            tr("Camera Removed Detected"),
            tr("Camera %1 is being unplugged.\nRecording will be stopped.").arg(id)
        );
    }

    _record_settings->remove_camera(id);

    if (auto it = _camera_window_map.find(id); it != _camera_window_map.end()) {
        auto stream_window = it->second;
        disconnect(stream_window, nullptr, this, nullptr);
        stream_window->close();
        stream_window->deleteLater();
        _camera_window_map.erase(it);
    }

    if (auto it = _camera_item_map.find(id); it != _camera_item_map.end()) {
        auto item = it->second;
        _camera_item_map.erase(it);
        delete _camera_list->takeItem(_camera_list->row(item));
    }

    _cameras.erase(
        std::remove_if(
            _cameras.begin(),
            _cameras.end(),
            [id](auto const &camera) {
                if (camera->id() == id) {
                    delete camera;
                    return true;
                };
                return false;
            }
        ),
        _cameras.end()
    );

    if (_camera_window_map.empty()) {
        _stream_mainwindow->close();
    } else {
        _stream_mainwindow->adjustSize();
    }
    _record_button->setEnabled(!_camera_window_map.empty());
};