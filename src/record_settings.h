#pragma once

#include <QCloseEvent>
// #include <QWidget>
#include <QDialog>
#include <QListWidget>
#include <vector>

#include "camera.h"
#include "camera_record_widget.h"
#include <QPoint>


class RecordSettings : public QDialog
{
    Q_OBJECT

public:
    RecordSettings(const std::vector<Camera *> &_cameras, QWidget *parent = nullptr);
    ~RecordSettings() = default;

private:
    std::vector<Camera *> cameras;
    std::vector<CameraRecordWidget *> camera_record_widgets;
    void load_cameras();
    QListWidget *cameras_list;
    QPoint start_p;

protected:
    void closeEvent(QCloseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

private slots:
    void on_ok_clicked();
    void load_settings();

    // void update_play_time();
    // void open_video_stream();
    // void play_all();
    // void stop_all();
    // void record_all();
    // void open_all();
};