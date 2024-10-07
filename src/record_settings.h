#pragma once

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QListWidget>
#include <QPoint>
#include <QWidget>
#include <vector>

#include "../src/camera.h"


class RecordSettings : public QDialog
{
    Q_OBJECT

public:
    RecordSettings(const std::vector<Camera *> &_cameras, QWidget *parent = nullptr);
    ~RecordSettings() = default;
    QListWidget *cameras_list;

private:
    std::vector<Camera *> cameras;
    QPoint start_p;
    QComboBox *save_path;
    QComboBox *dir_name;
    QCheckBox *additional_metadata;

protected:
    void closeEvent(QCloseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
};