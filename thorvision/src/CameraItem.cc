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

    auto parse_cap = [](const QString &s) {
        QRegularExpression re(R"((\d+)x(\d+)\s*@\s*([\d\.]+)FPS)");
        auto m = re.match(s);
        return std::tuple<int, int, double>{
            m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toDouble()
        };
    };

    for (const auto &cap : _camera->caps()) {
        const auto fps = static_cast<double>(cap.fps_n) / cap.fps_d;
        const auto &cap_str =
            QString("%1x%2 @ %3FPS").arg(cap.width).arg(cap.height).arg(format_fps(fps));

        QString codec_str;
        if (cap.media_type == "image/jpeg") {
            codec_str = tr("M-JPEG");
        } else if (cap.media_type == "video/x-h265") {
            codec_str = tr("H.265");
        } else if (cap.media_type == "video/x-h264") {
            codec_str = tr("H.264");
        }
        spdlog::debug("Added cap: {}, codec: {}", cap_str.toStdString(), codec_str.toStdString());

        _quality_format[{cap_str, codec_str}] = cap;
        caps.insert(cap_str);
        codecs.insert(codec_str);
    }

    _caps = QVector<QString>(caps.begin(), caps.end());
    _codecs = QVector<QString>(codecs.begin(), codecs.end());

    std::sort(_caps.begin(), _caps.end(), [&](const QString &a, const QString &b) {
        auto [wa, ha, fa] = parse_cap(a);
        auto [wb, hb, fb] = parse_cap(b);

        if (wa != wb) return wa > wb;
        if (ha != hb) return ha > hb;
        return fa > fb;
    });

    static const QMap<QString, int> codec_priority = {
        {tr("M-JPEG"), 0},
        {tr("H.265"), 1},
    };
    std::sort(_codecs.begin(), _codecs.end(), [&](const QString &a, const QString &b) {
        return codec_priority.value(a, 99) < codec_priority.value(b, 99);
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
    _stream->start(gst_cap.media_type);
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

    _stream->start(gst_cap.media_type);
    _camera->start(gst_cap);
}

void CameraItem::update_metadata(const XDAQFrameData &metadata)
{
    // spdlog::info("XDAQ Timestamp: {}", metadata.fpga_timestamp);

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

    if (_codec == tr("H.265")) {
        _stream->start_h265_recording(
            filepath,
            settings->split_on(),
            settings->split_length(),
            to_time_unit(settings->split_unit_index())
        );
    } else if (_codec == "M-JPEG") {
        xvc::start_jpeg_recording(
            GST_PIPELINE(_stream->_pipeline),
            filepath,
            settings->split_on(),
            settings->split_length(),
            to_time_unit(settings->split_unit_index())
        );
    } else {
        spdlog::warn("Unsupported codec: {}", _codec.toStdString());
    }
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

    std::chrono::seconds duration = std::chrono::seconds(10);
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
    return xvc::start_jpeg_recording(_stream->pipeline(), config);
}

bool CameraItem::stop_recording()
{
    spdlog::info("Stopping recording for camera {}", _camera->id());

    if (_codec == tr("H.265")) {
        _stream->stop_h265_recording();
    } else {
        xvc::stop_jpeg_recording(GST_PIPELINE(_stream->_pipeline));
    }
}