#pragma once

#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QtGui>

#include "CameraItem.h"

class CameraModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum {
        NameRole = Qt::UserRole + 1,
        CapRole,
        CodecRole,
    };

    explicit CameraModel(QObject *parent = 0);
    ~CameraModel();

    void add_camera(
        const QString &name, const QVector<QString> &caps, const QVector<QString> &codecs
    );
    // void remove_camera(const int index);
    // void update_camera();

    Q_INVOKABLE void set_name(const int index, const QString &name);
    Q_INVOKABLE void set_cap(const int index, const QString &cap);
    Q_INVOKABLE void set_codec(const int index, const QString &codec);

    Q_INVOKABLE QString name(int index) const;
    Q_INVOKABLE QStringList caps(int index) const;
    Q_INVOKABLE QStringList codecs(int index) const;

public:  // QAbstractItemModel interface
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

private:
    QList<CameraItem> _cameras;
};

#endif