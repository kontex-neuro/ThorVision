#include "CameraItem.h"

#include <spdlog/spdlog.h>

#include "xdaqvc/xvc.h"

CameraItem::CameraItem() {}

CameraItem::CameraItem(Camera *camera, ImageProvider *provider)
    : _camera(camera), _provider(provider), _cap(""), _codec("")
{
    _video_sink = new GstVideoSink(camera);
    _video_sink->setImageProvider(provider);

    QSet<QString> seen_caps, seen_codecs;

    auto format_fps = [](double fps) -> QString {
        return (fps == static_cast<int>(fps)) ? QString::number(static_cast<int>(fps))
                                              : QString::number(fps, 'f', 2);
    };

    _caps.append("");
    _codecs.append("");

    for (const auto &cap : _camera->caps()) {
        auto fps = static_cast<double>(cap.fps_n) / cap.fps_d;

        auto cap_str = QString("%1x%2 @ %3FPS").arg(cap.width).arg(cap.height).arg(format_fps(fps));

        for (const auto &codec : _camera->codecs()) {
            QString codec_str;
            switch (codec) {
            case Camera::Codec::M_JPEG: codec_str = "M-JPEG"; break;
            default: codec_str = "Unknown";
            }
            _quality_format[{cap_str, codec_str}] = cap;

            if (!seen_codecs.contains(codec_str)) {
                _codecs.append(codec_str);
                seen_codecs.insert(codec_str);
            }
        }

        if (!seen_caps.contains(cap_str)) {
            _caps.append(cap_str);
            seen_caps.insert(cap_str);
        }
    }
}

CameraItem::~CameraItem()
{
    _video_sink->deleteLater();
    delete _camera;
}

void CameraItem::set_name(const QString &name)
{
    spdlog::info("setName() = {}", name.toStdString());
    _camera->set_name(name.toStdString());
    emit name_changed();
}

void CameraItem::set_cap(const QString &cap)
{
    spdlog::info(
        "id = {}, name = {}, setCap() = {}", _camera->id(), name().toStdString(), cap.toStdString()
    );
    _cap = cap;
    emit cap_changed();

    if (!_codec.isEmpty() && _quality_format.contains({_cap, _codec})) {
        const auto &gst_cap = _quality_format[{_cap, _codec}];
        spdlog::info("setCap() = {}", gst_cap.to_string());

        _camera->start(gst_cap);
        _video_sink->startPipeline();
    }
}

void CameraItem::set_codec(const QString &codec)
{
    spdlog::info(
        "id = {}, name = {}, setCodec() = {}",
        _camera->id(),
        name().toStdString(),
        codec.toStdString()
    );
    _codec = codec;
    emit codec_changed();

    if (!_cap.isEmpty() && _quality_format.contains({_cap, _codec})) {
        const auto &gst_cap = _quality_format[{_cap, _codec}];
        spdlog::info("setCodec() = {}", gst_cap.to_string());

        _camera->set_stream_codec(Camera::Codec::M_JPEG);
        _camera->start(gst_cap);
        _video_sink->startPipeline();
    }
}

void CameraItem::update_metadata(const int camera_id)
{
    _metadata = _provider->metadata(QString::number(camera_id));
    emit metadata_changed();
}

void CameraItem::start_recording(RecorderSettings *settings)
{
    if (!settings) {
        spdlog::warn("RecorderSettings is null, cannot start recording");
        return;
    }

    auto to_time_unit = [](int index) {
        switch (index) {
        case 0: return xvc::TimeUnit::Seconds;
        case 1: return xvc::TimeUnit::Minutes;
        case 2: return xvc::TimeUnit::Hours;
        case 3: return xvc::TimeUnit::Days;
        default: return xvc::TimeUnit::Minutes;
        }
    };

    auto filepath = fs::path(settings->save_paths().at(0).toStdString()) /
                    settings->dir_name().toStdString() / _camera->name();

    xvc::start_jpeg_recording(
        GST_PIPELINE(_video_sink->pipeline()),
        filepath,
        settings->split_on(),
        settings->split_length(),
        to_time_unit(settings->split_unit_index()),
        settings->loop_on(),
        settings->max_files()
    );
}

void CameraItem::stop_recording()
{
    spdlog::info("Stopping recording for camera {}", _camera->id());

    xvc::stop_jpeg_recording(GST_PIPELINE(_video_sink->pipeline()));
}