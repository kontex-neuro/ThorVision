#include "Recorder.h"

#include "spdlog/spdlog.h"

Recorder::Recorder(CameraModel *camera_model, RecorderSettings *settings, QObject *parent)
    : QObject(parent), _recording(false), _time_seconds(0), _recording_time("00:00:00")
{
    _camera_model = camera_model;
    _settings = settings;
    _timer = new QTimer(this);
    _timer->setInterval(1000);

    connect(_timer, &QTimer::timeout, this, [this]() {
        ++_time_seconds;
        auto hrs = _time_seconds / 3600;
        auto mins = (_time_seconds % 3600) / 60;
        auto secs = _time_seconds % 60;

        _recording_time = QString("%1:%2:%3")
                              .arg(hrs, 2, 10, QChar('0'))
                              .arg(mins, 2, 10, QChar('0'))
                              .arg(secs, 2, 10, QChar('0'));

        spdlog::info("_recording_time: {}", _recording_time.toStdString());
        emit recording_time_changed();
    });
}

void Recorder::start()
{
    spdlog::info("Recorder::start");

    _recording = true;
    _time_seconds = 0;
    _recording_time = "00:00:00";
    _timer->start();

    _settings->set_dir_name(
        _settings->dir_date() ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss")
                              : _settings->dir_name()
    );

    for (auto i = 0; i < _camera_model->count(); i++) {
        auto camera_item = _camera_model->get(i)["camera_item"].value<CameraItem *>();
        if (!camera_item) {
            spdlog::warn("Camera item at index {} is null, skipping", i);
            continue;
        }

        camera_item->start_recording(_settings);
    }

    emit recording_changed();
    emit recording_time_changed();
}

void Recorder::stop()
{
    spdlog::info("Stopping recording");

    _recording = false;
    _timer->stop();

    for (auto i = 0; i < _camera_model->count(); i++) {
        auto camera_item = _camera_model->get(i)["camera_item"].value<CameraItem *>();
        if (!camera_item) {
            spdlog::warn("Camera item at index {} is null, skipping", i);
            continue;
        }

        camera_item->stop_recording();
    }

    emit recording_changed();
    emit recording_time_changed();
}