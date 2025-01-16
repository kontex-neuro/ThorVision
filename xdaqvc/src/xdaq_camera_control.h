#pragma once

#include <QCloseEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <future>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "record_settings.h"
#include "stream_mainwindow.h"
#include "xdaqvc/camera.h"
#include "xdaqvc/ws_client.h"


class XDAQCameraControl : public QMainWindow
{
    Q_OBJECT

public:
    explicit XDAQCameraControl();
    ~XDAQCameraControl() = default;
    StreamMainWindow *_stream_mainwindow;
    std::vector<Camera *> _cameras;

    RecordSettings *_record_settings;
    QPushButton *_record_button;
    QListWidget *_camera_list;
    QLabel *_record_time;
    QTimer *_timer;
    int _elapsed_time;

private:
    std::vector<std::pair<std::thread, std::future<void>>> parsing_threads;
    bool are_threads_finished() const;
    void wait_for_threads();
    void cleanup_finished_threads();
    std::unique_ptr<xvc::ws_client> _ws_client;
    std::unordered_map<int, QListWidgetItem *> _camera_item_map;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
};