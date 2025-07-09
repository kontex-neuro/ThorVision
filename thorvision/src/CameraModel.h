#pragma once

#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QtGui>

struct CameraItem {
    QString name;
    QString cap;
    QString codec;
};

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

    void add_camera(const QString &name);

public:  // QAbstractItemModel interface
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

private:
    QList<QString> m_data;
};

#endif