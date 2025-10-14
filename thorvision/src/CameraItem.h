#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <gst/gst.h>
#include <gst/gstelement.h>

#include <QHash>
#include <QString>
#include <QVector>
#include <memory>
#include <optional>

#include "RecorderSettings.h"
#include "xdaqmetadata/metadata_handler.h"
#include "xdaqmetadata/xdaqmetadata.h"
#include "xdaqvc/camera.h"

struct Stream {
    GstPipeline *_pipeline;
    int _index;
    std::optional<GstClockTime> _base_time;
    std::unique_ptr<MetadataHandler> _metadata_handler;

    Stream(GstPipeline *pipeline, int index) : _pipeline(pipeline), _index(index)
    {
        _metadata_handler = std::make_unique<MetadataHandler>();
    }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&stream) noexcept : _pipeline(stream._pipeline) { stream._pipeline = nullptr; }
    Stream &operator=(Stream &&stream) noexcept
    {
        if (this == &stream) return *this;
        _pipeline = std::exchange(stream._pipeline, nullptr);
        return *this;
    }

    void start()
    {
        if (_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING);
        }
    }

    ~Stream()
    {
        if (_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);
            gst_object_unref(_pipeline);
        }
    }
};

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
    QString ttl_out() const { return QString::number(_metadata.ttl_out); };

    void update_metadata(const XDAQFrameData &metadata);

    void start_recording(RecorderSettings *settings);
    void stop_recording();

    int port() const { return _camera->port(); };
    std::unique_ptr<Stream> _stream;

signals:
    void name_changed();
    void cap_changed();
    void codec_changed();
    void caps_changed();
    void codecs_changed();
    void metadata_changed();

private:
    Camera *_camera;

    QHash<std::pair<QString, QString>, Camera::Cap> _quality_format;
    QVector<QString> _caps;
    QVector<QString> _codecs;
    QString _cap;
    QString _codec;
    XDAQFrameData _metadata;
};

#endif