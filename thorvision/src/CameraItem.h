#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>
#include <utility>

#include "RecorderSettings.h"
#include "Stream.h"
#include "xdaqmetadata/xdaqmetadata.h"
#include "xdaqvc/camera.h"

class CameraItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE set_name NOTIFY name_changed)
    Q_PROPERTY(QString cap READ cap WRITE set_cap NOTIFY cap_changed)
    Q_PROPERTY(QString codec READ codec WRITE set_codec NOTIFY codec_changed)
    Q_PROPERTY(QVector<QString> caps READ caps NOTIFY caps_changed)
    Q_PROPERTY(QVector<QString> codecs READ codecs NOTIFY codecs_changed)
    Q_PROPERTY(QString xdaq_timestamp READ xdaq_timestamp NOTIFY metadata_changed)

public:
    explicit CameraItem(std::unique_ptr<Camera> camera, QObject *parent = nullptr);
    ~CameraItem();

    int id() const noexcept { return _camera->id(); };
    int port() const noexcept { return _camera->port(); };
    QString name() const noexcept { return QString::fromStdString(_camera->name()); };
    QString device_id() const noexcept { return QString::fromStdString(_camera->device_id()); };
    const QVector<QString> &caps() const noexcept { return _caps; };
    const QVector<QString> &codecs() const noexcept { return _codecs; };
    const QString &cap() const noexcept { return _cap; };
    const QString &codec() const noexcept { return _codec; };
    const QString &default_cap() const noexcept { return _caps.at(_caps.size() - 1); };
    QString default_codec() const noexcept { return tr("MJPEG"); };
    QString xdaq_timestamp() const noexcept { return QString::number(_metadata.fpga_timestamp); };
    // TODO
    bool streaming() const noexcept { return _stream->streaming(); };

    void set_name(const QString &name);
    void set_cap(const QString &cap);
    void set_codec(const QString &codec);

    void update_metadata(const XDAQFrameData &metadata);

    Q_INVOKABLE bool cap_selectable(const QString &cap) const noexcept
    {
        return _codec.isEmpty() || cap.isEmpty() || _quality_format.contains({cap, _codec});
    };
    Q_INVOKABLE bool codec_selectable(const QString &codec) const noexcept
    {
        return _cap.isEmpty() || codec.isEmpty() || _quality_format.contains({_cap, codec});
    };
    Q_INVOKABLE QString cap_display(const QString &cap) const noexcept
    {
        return (cap == default_cap()) ? cap + " (default)" : cap;
    }
    Q_INVOKABLE QString codec_display(const QString &codec) const noexcept
    {
        return (codec == default_codec()) ? codec + " (default)" : codec;
    }
    void set_stream(std::unique_ptr<Stream> stream)
    {
        _stream = std::move(stream);
        connect(
            _stream.get(), &Stream::metadata_received, this, [this](const XDAQFrameData &metadata) {
                _metadata = metadata;
                emit metadata_changed();
            }
        );
        connect(_stream.get(), &Stream::status_changed, this, [this](bool streaming) {
            emit stream_status_changed(streaming);
        });
    };

    bool start_recording(const RecorderSettings &settings);
    bool stop_recording();

signals:
    void name_changed();
    void cap_changed();
    void codec_changed();
    void caps_changed();
    void codecs_changed();
    void metadata_changed();
    void stream_status_changed(bool streaming);

private:
    std::unique_ptr<Camera> _camera;
    std::unique_ptr<Stream> _stream;

    QHash<std::pair<QString, QString>, Camera::Cap> _quality_format;
    QVector<QString> _caps;
    QVector<QString> _codecs;
    QString _cap;
    QString _codec;
    XDAQFrameData _metadata;
};