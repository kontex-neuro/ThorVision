#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QQuickItem>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <memory>

#include "CameraItem.h"
#include "xdaqvc/camera.h"

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

    void add_camera(std::unique_ptr<Camera> camera);
    void remove_camera(const int index);
    int index_of_camera_id(const int id) const noexcept;
    bool set_name(int index, const QString &value) const;

    int selected_camera_index() const noexcept { return _selected_camera_index; };
    void set_selected_camera_index(const int index) noexcept
    {
        if (_selected_camera_index == index) return;
        _selected_camera_index = index;
        emit selected_camera_changed(_selected_camera_index);
    };

    Q_INVOKABLE bool all_cameras_streaming() const;
    Q_INVOKABLE QVariantMap get(const int index) const noexcept;

public:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

signals:
    void selected_camera_changed(int index);
    void camera_count_changed();
    void camera_unplugged_during_recording(const QString &camera_name);
    void all_cams_streaming();

public slots:
    void onItemAdded(int index, QQuickItem *item);
    void onItemRemoved(int index, QQuickItem *item);

private:
    QList<CameraItem *> _cameras;
    int _selected_camera_index;

    QString unique_camera_name(const QString &base, int self_index) const;
};