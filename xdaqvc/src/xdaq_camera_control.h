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
#include <vector>

#include "record_settings.h"
#include "stream_mainwindow.h"
#include "xdaqvc/camera.h"
#include "xdaqvc/ws_client.h"



class XDAQCameraControl : public QMainWindow
{
    Q_OBJECT

public:
    XDAQCameraControl();
    ~XDAQCameraControl() = default;
    StreamMainWindow *stream_mainwindow;
    std::vector<Camera *> cameras;

    void load_cameras();
    void mock_camera();

    RecordSettings *record_settings;
    QPushButton *record_button;
    QListWidget *cameras_list;
    QLabel *record_time;
    QTimer *timer;
    int elapsed_time;

private:
    std::vector<std::pair<std::thread, std::future<void>>> parsing_threads;
    bool are_threads_finished() const;
    void wait_for_threads();
    void cleanup_finished_threads();
    std::unique_ptr<xvc::ws_client> _ws_client;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
};