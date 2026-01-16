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
    Q_PROPERTY(bool api_control READ api_control WRITE set_api_control NOTIFY api_control_changed)
    Q_PROPERTY(
        QString api_controller_name READ api_controller_name WRITE set_api_controller_name NOTIFY
            api_controller_name_changed
    )

public:
    explicit Recorder(
        CameraModel *camera_model, RecorderSettings *settings, QObject *parent = nullptr
    );
    ~Recorder() = default;

    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();

    Q_INVOKABLE bool recording() const { return _recording; }
    QString recording_time() const { return _recording_time; }

    Q_INVOKABLE bool api_control() const { return _api_control; }
    Q_INVOKABLE void set_api_control(bool value)
    {
        if (_api_control == value) return;

        _api_control = value;
        emit api_control_changed();
    };

    Q_INVOKABLE QString api_controller_name() const { return _api_controller_name; }
    Q_INVOKABLE void set_api_controller_name(QString name)
    {
        if (_api_controller_name == name) return;

        _api_controller_name = name;
        emit api_controller_name_changed();
    };

signals:
    void recording_changed();
    void recording_time_changed();
    void settings_changed();
    void api_control_changed();
    void api_controller_name_changed();

private:
    RecorderSettings *_settings;
    CameraModel *_camera_model;

    bool _recording;
    int _time_seconds;
    QString _recording_time;
    QTimer *_timer;

    bool _api_control;
    QString _api_controller_name;
};

#endif