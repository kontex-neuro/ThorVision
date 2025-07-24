#include "Recorder.h"

#include "spdlog/spdlog.h"
#include "xdaqvc/xvc.h"

Recorder::Recorder(QObject *parent) : QObject(parent) {}

Recorder::~Recorder() {}

void Recorder::start()
{
    spdlog::info("Recorder::start");

    _recording = true;
    emit recording_changed();
    // xvc::start_jpeg_recording(GstPipeline * pipeline, fs::path & filepath);
}

void Recorder::stop()
{
    spdlog::info("Recorder::start");

    _recording = false;
    emit recording_changed();
    // xvc::stop_jpeg_recording(GstPipeline * pipeline);
}