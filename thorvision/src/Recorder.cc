#include "Recorder.h"

#include <spdlog/spdlog.h>

#include <QDateTime>

Recorder::Recorder(CameraModel &camera_model, QObject *parent)
    : QObject(parent),
      _camera_model(&camera_model),
      _recording(false),
      _time_seconds(0),
      _api_control(false)
{
    _timer.setInterval(1000);

    connect(&_timer, &QTimer::timeout, this, [this]() {
        ++_time_seconds;
        const auto hrs = _time_seconds / 3600;
        const auto mins = (_time_seconds % 3600) / 60;
        const auto secs = _time_seconds % 60;

        _recording_time = QString("%1:%2:%3")
                              .arg(hrs, 2, 10, QChar('0'))
                              .arg(mins, 2, 10, QChar('0'))
                              .arg(secs, 2, 10, QChar('0'));

        emit recording_time_changed();
    });
}

bool Recorder::start()
{
    if (_recording) {
        spdlog::warn("Camera already recording, ignoring start request.");
        return false;
    }

    for (auto i = 0; i < _camera_model->rowCount(); ++i) {
        auto camera_item = _camera_model->get(i)["camera_item"].value<CameraItem *>();
        if (!camera_item) {
            spdlog::warn("Camera item at index {} is null, skipping", i);
            continue;
        }

        auto result = camera_item->start_recording(settings);
        if (!result) {
            emit failed_to_start_recording(
                "The selected location is invalid to write. Re-select a valid location"
            );
            return false;
        }
    }

    _recording = true;
    _time_seconds = 0;
    _recording_time = "00:00:00";
    _timer.start();

    emit recording_changed();
    emit recording_time_changed();
    return true;
}

bool Recorder::stop()
{
    if (!_recording) {
        spdlog::warn("Camera is not recording, ignoring stop request");
        return false;
    }

    for (auto i = 0; i < _camera_model->rowCount(); ++i) {
        auto camera_item = _camera_model->get(i)["camera_item"].value<CameraItem *>();
        if (!camera_item) {
            spdlog::warn("Camera item at index {} is null, skipping", i);
            continue;
        }

        camera_item->stop_recording();
    }

    _recording = false;
    _timer.stop();

    emit recording_changed();
    emit recording_time_changed();
    return true;
}