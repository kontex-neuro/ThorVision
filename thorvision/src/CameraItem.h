#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <QHash>
#include <QString>
#include <QVector>
#include <chrono>

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
    Q_PROPERTY(QString rhythm_timestamp READ rhythm_timestamp NOTIFY metadata_changed)
    Q_PROPERTY(QString ttl_in READ ttl_in NOTIFY metadata_changed)
    Q_PROPERTY(QString ttl_out READ ttl_out NOTIFY metadata_changed)

public:
    explicit CameraItem(Camera *camera, QObject *parent = nullptr);
    ~CameraItem();

    int id() const { return _camera->id(); };

    Q_INVOKABLE QString name() const { return QString::fromStdString(_camera->name()); };
    Q_INVOKABLE void set_name(const QString &name);

    Q_INVOKABLE QVector<QString> caps() const { return _caps; };
    Q_INVOKABLE QVector<QString> codecs() const { return _codecs; };

    Q_INVOKABLE bool cap_selectable(const QString &cap) const;
    Q_INVOKABLE bool codec_selectable(const QString &codec) const;

    Q_INVOKABLE QString cap() const { return _cap; };
    Q_INVOKABLE void set_cap(const QString &cap);

    Q_INVOKABLE QString codec() const { return _codec; };
    Q_INVOKABLE void set_codec(const QString &codec);

    QString xdaq_timestamp() const { return QString::number(_metadata.fpga_timestamp); };
    QString rhythm_timestamp() const { return QString::number(_metadata.rhythm_timestamp); };
    QString ttl_in() const { return QString::number(_metadata.ttl_in); };
    QString ttl_out() const { return QString::number(_metadata.ttl_out); };

    void update_metadata(const XDAQFrameData &metadata);

    void start_recording(RecorderSettings *settings);
    void stop_recording();

    int port() const { return _camera->port(); };

    void set_stream(std::unique_ptr<Stream> stream)
    {
        _stream = std::move(stream);
        connect(_stream.get(), &Stream::metadata_received, this, &CameraItem::update_metadata);
        connect(_stream.get(), &Stream::status_changed, this, [this](bool streaming) {
            emit stream_status_changed(streaming);
        });
    };

    bool is_streaming() const { return _stream ? _stream->_streaming.load() : false; };

    void cleanup_stream()
    {
        if (_stream) {
            _stream->reset();
        }
    }

signals:
    void name_changed();
    void cap_changed();
    void codec_changed();
    void caps_changed();
    void codecs_changed();
    void metadata_changed();
    void stream_status_changed(bool streaming);

private:
    Camera *_camera;
    std::unique_ptr<Stream> _stream;

    QHash<std::pair<QString, QString>, Camera::Cap> _quality_format;
    QVector<QString> _caps;
    QVector<QString> _codecs;
    QString _cap;
    QString _codec;
    XDAQFrameData _metadata;
};

#endif