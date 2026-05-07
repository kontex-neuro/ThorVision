#include "CameraItem.h"

#include <spdlog/spdlog.h>

#include <QQuickItem>
#include <QRegularExpression>
#include <QSet>
#include <tuple>

#include "xdaqvc/xvc.h"

CameraItem::CameraItem(std::unique_ptr<Camera> camera, QObject *parent)
    : QObject(parent), _camera(std::move(camera)), _metadata(XDAQFrameData{0, 0, 0, 0, 0, 0})
{
    QSet<QString> caps, codecs;

    auto format_fps = [](double fps) -> QString {
        return (fps == static_cast<int>(fps)) ? QString::number(static_cast<int>(fps))
                                              : QString::number(fps, 'f', 2);
    };

    for (const auto &cap : _camera->caps()) {
        const auto fps = static_cast<double>(cap.fps_n) / cap.fps_d;
        const auto &cap_str =
            QString("%1x%2 @ %3FPS").arg(cap.width).arg(cap.height).arg(format_fps(fps));

        QString codec_str;
        if (cap.media_type == "image/jpeg") {
            codec_str = tr("MJPEG");
        } else if (cap.media_type == "video/x-h265") {
            codec_str = tr("H.265");
        }
        spdlog::trace("Added cap: {}, codec: {}", cap_str.toStdString(), codec_str.toStdString());

        _quality_format[{cap_str, codec_str}] = cap;
        caps.insert(cap_str);
        codecs.insert(codec_str);
    }

    _caps = QVector<QString>(caps.begin(), caps.end());
    _codecs = QVector<QString>(codecs.begin(), codecs.end());

    std::sort(_caps.begin(), _caps.end(), [](const QString &a, const QString &b) {
        auto parse_cap = [](const QString &s) {
            QRegularExpression re(R"((\d+)x(\d+)\s*@\s*([\d\.]+)FPS)");
            auto m = re.match(s);
            return std::tuple<int, int, double>{
                m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toDouble()
            };
        };

        auto [wa, ha, fa] = parse_cap(a);
        auto [wb, hb, fb] = parse_cap(b);

        if (wa != wb) return wa > wb;
        if (ha != hb) return ha > hb;
        return fa > fb;
    });

    std::sort(_codecs.begin(), _codecs.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });

    _caps.insert(0, "");
    _codecs.insert(0, "");
}

CameraItem::~CameraItem()
{
    if (!_camera) return;
    _camera->stop();
}

void CameraItem::set_name(const QString &name)
{
    _camera->set_name(name.toStdString());
    emit name_changed();
}

void CameraItem::set_cap(const QString &cap)
{
    if (_cap == cap) return;
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

    _stream->start(gst_cap.media_type);
    _camera->start(gst_cap);
}

void CameraItem::set_codec(const QString &codec)
{
    if (_codec == codec) return;
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

    _stream->start(gst_cap.media_type);
    _camera->start(gst_cap);
}

void CameraItem::update_metadata(const XDAQFrameData &metadata)
{
    if (_metadata.fpga_timestamp == metadata.fpga_timestamp) return;
    _metadata = metadata;
    emit metadata_changed();
}

bool CameraItem::start_recording(const RecorderSettings &settings)
{
    const auto split = settings.split_on();
    const auto split_length = settings.split_length();
    const auto split_unit = settings.split_unit_index();

    const auto &parent_dir = std::filesystem::path(settings.save_paths().at(0).toStdString());
    const auto &dir_name =
        settings.dir_date()
            ? std::filesystem::path(
                  QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss").toStdString()
              )
            : std::filesystem::path(settings.dir_name().toStdString());
    const auto &record_dir = parent_dir / dir_name;
    const auto &filepath = record_dir / _camera->name();

    auto duration = std::chrono::seconds(10);
    if (split) {
        switch (split_unit) {
        case 0: duration = std::chrono::seconds(split_length); break;
        case 1: duration = std::chrono::minutes(split_length); break;
        case 2: duration = std::chrono::hours(split_length); break;
        case 3: duration = std::chrono::days(split_length); break;
        default: spdlog::warn("Invalid split unit index"); break;
        }
    }

    std::error_code ec;
    if (!std::filesystem::exists(record_dir, ec) &&
        !std::filesystem::create_directories(record_dir, ec)) {
        spdlog::critical("Failed to create directory {}: {}", record_dir.string(), ec.message());
        return false;
    }

    xvc::RecordConfig config(filepath, split, duration);

    if (_codec == tr("H.265")) {
        return _stream->start_h265_recording(config);
    } else if (_codec == "MJPEG") {
        return xvc::start_jpeg_recording(_stream->_pipeline, config);
    }
    return false;
}

bool CameraItem::stop_recording()
{
    spdlog::info("Stopping recording for camera {}", _camera->id());

    if (_codec == tr("H.265")) {
        return _stream->stop_h265_recording();
    } else if (_codec == "MJPEG") {
        return xvc::stop_jpeg_recording(_stream->_pipeline);
    }
    return false;
}
