#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <QtGui>

#include "GstVideoSink.h"
#include "ImageProvider.h"
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
    Q_PROPERTY(QString ttl_out READ ttl_out NOTIFY metadata_changed)

public:
    explicit CameraItem();
    CameraItem(Camera *camera, ImageProvider *provider);
    ~CameraItem();

    int id() const;

    Q_INVOKABLE QString name() const;
    Q_INVOKABLE void set_name(const QString &name);

    Q_INVOKABLE QVector<QString> caps() const;

    Q_INVOKABLE QVector<QString> codecs() const;

    Q_INVOKABLE QString cap() const;
    Q_INVOKABLE void set_cap(const QString &cap);

    Q_INVOKABLE QString codec() const;
    Q_INVOKABLE void set_codec(const QString &codec);

    Q_INVOKABLE QString xdaq_timestamp() const;
    Q_INVOKABLE QString rhythm_timestamp() const;
    Q_INVOKABLE QString ttl_out() const;

    Q_INVOKABLE void update_metadata(const int camera_id);

signals:
    void name_changed();
    void cap_changed();
    void codec_changed();
    void caps_changed();
    void codecs_changed();
    void metadata_changed();

private:
    Camera *_camera;
    GstVideoSink *_video_sink;
    ImageProvider *_provider;

    QHash<std::pair<QString, QString>, Camera::Cap> _quality_format;
    QVector<QString> _caps;
    QVector<QString> _codecs;
    QString _cap;
    QString _codec;
    XDAQFrameData _metadata;
};

#endif