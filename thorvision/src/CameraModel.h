#pragma once

#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QQuickItem>
#include <QtGui>

#include "CameraItem.h"

class CameraModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(
        int selected_camera_index READ selected_camera_index WRITE set_selected_camera_index NOTIFY
            selected_camera_changed
    )
    Q_PROPERTY(int rowCount READ rowCount NOTIFY camera_count_changed)
    Q_PROPERTY(bool all_cameras_streaming READ all_cameras_streaming NOTIFY all_cams_streaming)

public:
    enum {
        IdRole = Qt::UserRole + 1,
        CameraItemRole,
        NameRole,
        CapRole,
        CodecRole,
        CapsRole,
        CodecsRole
    };

    explicit CameraModel(QObject *parent = nullptr);
    ~CameraModel();

    void add_camera(Camera *camera);
    void remove_camera(const int index);
    // TODO: to find camera index by id
    int index_of_camera_id(const int id) const;

    Q_INVOKABLE QVariantMap get(const int index) const;
    // Q_INVOKABLE void set(int index) const;

    Q_INVOKABLE int selected_camera_index() const { return _selected_camera_index; };
    Q_INVOKABLE void set_selected_camera_index(const int index);

    Q_INVOKABLE bool set_name(int index, const QString &value) const;

    bool all_cameras_streaming() const;

    QString unique_camera_name(const QString &base, int self_index) const;

public:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE bool setData(
        const QModelIndex &index, const QVariant &value, int role = Qt::EditRole
    ) override;

signals:
    void selected_camera_changed();
    void camera_count_changed();
    void camera_unplugged_during_recording(const QString &camera_name);
    void all_cams_streaming();

public slots:
    void onItemAdded(int index, QQuickItem *item);

private:
    QList<CameraItem *> _cameras;
    int _selected_camera_index;
};

#endif