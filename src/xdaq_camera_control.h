#pragma once

#include <QCloseEvent>
#include <QListWidget>
#include <QMainWindow>
#include <vector>

#include "camera.h"
#include "record_settings.h"
#include "stream_main_window.h"


class XDAQCameraControl : public QMainWindow
{
    Q_OBJECT

public:
    XDAQCameraControl();
    ~XDAQCameraControl() = default;
    StreamMainWindow *stream_main_window;
    std::vector<Camera *> cameras;

    void load_cameras();

private:
    // std::vector<StreamWidget *> stream_widgets;
    RecordSettings *record_settings;
    QPushButton *record_button;
    QListWidget *cameras_list;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // void update_play_time();
    void open_record_settings();
    void record();
};