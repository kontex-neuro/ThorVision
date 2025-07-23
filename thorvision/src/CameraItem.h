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
    Q_PROPERTY(QString cap READ current_cap WRITE set_cap NOTIFY cap_changed)
    Q_PROPERTY(QString codec READ current_codec WRITE set_codec NOTIFY codec_changed)
    Q_PROPERTY(QVector<QString> caps READ caps NOTIFY caps_changed)
    Q_PROPERTY(QVector<QString> codecs READ codecs NOTIFY codecs_changed)

public:
    explicit CameraItem();
    CameraItem(Camera *camera, ImageProvider *provider);
    ~CameraItem();

    int id() const;

    QString name() const;
    void set_name(const QString &name);

    QVector<QString> caps() const;
    void set_cap(const QString &cap);

    QVector<QString> codecs() const;
    void set_codec(const QString &codec);

    QString current_cap() const;
    QString current_codec() const;

signals:
    void name_changed();
    void cap_changed();
    void codec_changed();
    void caps_changed();
    void codecs_changed();

private:
    Camera *_camera;
    GstVideoSink *_video_sink;

    QHash<std::pair<QString, QString>, Camera::Cap> _quality_format;
    QVector<QString> _caps;
    QVector<QString> _codecs;
    QString _current_cap;
    QString _current_codec;
};

#endif