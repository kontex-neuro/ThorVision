#pragma once

#include <QCloseEvent>
#include <QListWidget>
#include <QMainWindow>
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

private:
    // std::vector<StreamWidget *> stream_widgets;
    RecordSettings *record_settings;
    QPushButton *record_button;
    QListWidget *cameras_list;
    int elapsed_time;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *e) override;
};