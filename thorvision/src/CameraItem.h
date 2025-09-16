#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <QtGui>

#include "GstVideoSink.h"
#include "ImageProvider.h"
#include "RecorderSettings.h"
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
    explicit CameraItem(QObject *parent = nullptr);
    CameraItem(Camera *camera, ImageProvider *provider, QObject *parent = nullptr);
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

    Q_INVOKABLE void update_metadata(const int camera_id);

    void start_recording(RecorderSettings *settings);
    void stop_recording();

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