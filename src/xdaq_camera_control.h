#pragma once

#include <QCloseEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QTimer>
#include <vector>

#include "../src/camera.h"
#include "record_settings.h"
#include "stream_mainwindow.h"


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
    // std::vector<StreamWidget *> stream_widgets;


protected:
    void mousePressEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
};