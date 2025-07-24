#pragma once

#ifndef RECORDER_H
#define RECORDER_H

#include <QtCore>

class Recorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recording_changed)

public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder();

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    Q_INVOKABLE bool recording() const { return _recording; }
    // void set_recording(bool recording) { _recording = recording; }

signals:
    void recording_changed();

private:
    bool _recording = false;
};

#endif