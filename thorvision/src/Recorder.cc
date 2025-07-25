#include "Recorder.h"

#include "spdlog/spdlog.h"
#include "xdaqvc/xvc.h"

Recorder::Recorder(QObject *parent)
    : QObject(parent), _recording(false), _time_seconds(0), _recording_time("00:00:00")
{
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
    // xvc::start_jpeg_recording(GstPipeline * pipeline, fs::path & filepath);

    emit recording_changed();
    emit recording_time_changed();
}

void Recorder::stop()
{
    spdlog::info("Recorder::stop");

    _recording = false;
    _timer->stop();
    // xvc::stop_jpeg_recording(GstPipeline * pipeline);

    emit recording_changed();
    emit recording_time_changed();
}