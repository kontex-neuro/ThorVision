#pragma once

#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <QString>
#include <QTimer>

#include "CameraModel.h"
#include "RecorderSettings.h"

class Recorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recording_changed)
    Q_PROPERTY(QString recording_time READ recording_time NOTIFY recording_time_changed)

public:
    explicit Recorder(
        CameraModel *camera_model, RecorderSettings *settings, QObject *parent = nullptr
    );
    ~Recorder() = default;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    Q_INVOKABLE bool recording() const { return _recording; }
    QString recording_time() const { return _recording_time; }

signals:
    void recording_changed();
    void recording_time_changed();
    void settings_changed();

private:
    bool _recording;
    int _time_seconds;
    QString _recording_time;
    QTimer *_timer;
    RecorderSettings *_settings;
    CameraModel *_camera_model;
};

#endif