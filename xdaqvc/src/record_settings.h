#pragma once

#include <QCloseEvent>
#include <QDialog>
#include <QListWidget>
#include <QPoint>
#include <QWidget>

#include "xdaqvc/camera.h"


class RecordSettings : public QDialog
{
    Q_OBJECT

public:
    RecordSettings(QWidget *parent = nullptr);
    ~RecordSettings() = default;
    void add_camera(Camera *camera);
    void remove_camera(int const id);

private:
    QPoint _start_p;
    QListWidget *_camera_list;
    std::unordered_map<int, QListWidgetItem *> _camera_item_map;

protected:
    void closeEvent(QCloseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
};