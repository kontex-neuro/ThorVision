#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <QtGui>

class CameraItem
{
public:
    explicit CameraItem();
    CameraItem(const QString &name, const QVector<QString> &caps, const QVector<QString> &codecs);
    ~CameraItem();

    QString name() const;
    void set_name(const QString &name);

    QVector<QString> caps() const;
    void set_cap(const QString &cap);

    QVector<QString> codecs() const;
    void set_codec(const QString &codec);

private:
    QString _name;
    QString _cap;
    QString _codec;

    QVector<QString> _caps;
    QVector<QString> _codecs;
};

#endif