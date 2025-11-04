#include "CameraItem.h"

#include <spdlog/spdlog.h>

#include <QQuickItem>
#include <memory>

#include "xdaqvc/xvc.h"

CameraItem::CameraItem(Camera *camera, QObject *parent)
    : QObject(parent),
      _camera(camera),
      _cap(""),
      _codec(""),
      _metadata(XDAQFrameData{0, 0, 0, 0, 0, 0})
{
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
            case Camera::Codec::MJPEG: codec_str = tr("M-JPEG"); break;
            // case Camera::Codec::H265: codec_str = tr("H.265"); break;
            default: codec_str = tr("M-JPEG");
            }
            // _quality_format[{cap_str, codec_str}] = cap;

            // if (!seen_codecs.contains(codec_str)) {
            //     _codecs.append(codec_str);
            //     seen_codecs.insert(codec_str);
            // }
            // || cap.media_type == "video/x-raw"/
            if ((codec == Camera::Codec::MJPEG && (cap.media_type == "image/jpeg"))) {
                // spdlog::info(
                //     "cap_str = {}, codec_str = {}", cap_str.toStdString(),
                //     codec_str.toStdString()
                // );
                _quality_format[{cap_str, codec_str}] = cap;

                if (!seen_codecs.contains(codec_str)) {
                    _codecs.append(codec_str);
                    seen_codecs.insert(codec_str);
                }

                if (!seen_caps.contains(cap_str)) {
                    _caps.append(cap_str);
                    seen_caps.insert(cap_str);
                }
            }
        }

        // if (!seen_caps.contains(cap_str)) {
        //     _caps.append(cap_str);
        //     seen_caps.insert(cap_str);
        // }
    }
}

CameraItem::~CameraItem()
{
    // TODO
    _camera->stop();
    delete _camera;
}

void CameraItem::set_name(const QString &name)
{
    spdlog::info("setName() = {}", name.toStdString());
    _camera->set_name(name.toStdString());
    emit name_changed();
}

bool CameraItem::cap_selectable(const QString &cap) const
{
    if (_codec.isEmpty() || cap.isEmpty()) {
        return true;
    }
    if (_quality_format.contains({cap, _codec})) {
        return true;
    }
    return false;
}

bool CameraItem::codec_selectable(const QString &codec) const
{
    if (_cap.isEmpty() || codec.isEmpty()) {
        return true;
    }
    if (_quality_format.contains({_cap, codec})) {
        return true;
    }
    return false;
}

void CameraItem::set_cap(const QString &cap)
{
    _cap = cap;
    emit cap_changed();
    if (_codec.isEmpty()) return;
    if (!_quality_format.contains({_cap, _codec})) return;

    spdlog::info(
        "CameraItem::set_cap() id = {}, name = {}, cap = {} codec = {}",
        _camera->id(),
        _camera->name(),
        _cap.toStdString(),
        _codec.toStdString()
    );
    const auto &gst_cap = _quality_format[{_cap, _codec}];
    _camera->start(gst_cap);
    _stream->start();
}

void CameraItem::set_codec(const QString &codec)
{
    _codec = codec;
    emit codec_changed();
    if (_cap.isEmpty()) return;
    if (!_quality_format.contains({_cap, _codec})) return;

    spdlog::info(
        "CameraItem::set_codec() id = {}, name = {}, cap = {} codec = {}",
        _camera->id(),
        _camera->name(),
        _cap.toStdString(),
        _codec.toStdString()
    );
    const auto &gst_cap = _quality_format[{_cap, _codec}];

    // TODO: needs to set codec first then start the pipeline
    _camera->set_stream_codec(Camera::Codec::MJPEG);
    _camera->start(gst_cap);
    _stream->start();
}

void CameraItem::update_metadata(const XDAQFrameData &metadata)
{
    _metadata = metadata;
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
        GST_PIPELINE(_stream->_pipeline),
        filepath,
        settings->split_on(),
        settings->split_length(),
        to_time_unit(settings->split_unit_index())
    );
}

void CameraItem::stop_recording()
{
    spdlog::info("Stopping recording for camera {}", _camera->id());

    xvc::stop_jpeg_recording(GST_PIPELINE(_stream->_pipeline));
}