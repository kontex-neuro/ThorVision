#include "CameraItem.h"

#include <spdlog/spdlog.h>

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
    delete _video_sink;
    delete _camera;
}

int CameraItem::id() const { return _camera->id(); }

QString CameraItem::name() const { return QString::fromStdString(_camera->name()); }

QVector<QString> CameraItem::caps() const { return _caps; }

QVector<QString> CameraItem::codecs() const { return _codecs; }

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

QString CameraItem::cap() const { return _cap; }

QString CameraItem::codec() const { return _codec; }

QString CameraItem::xdaq_timestamp() const { return QString::number(_metadata.fpga_timestamp); }

QString CameraItem::rhythm_timestamp() const { return QString::number(_metadata.rhythm_timestamp); }

QString CameraItem::ttl_out() const { return QString::number(_metadata.ttl_out); }

void CameraItem::update_metadata(const int camera_id)
{
    _metadata = _provider->metadata(QString::number(camera_id));
    emit metadata_changed();
}