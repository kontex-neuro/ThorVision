#pragma once

#include <QCloseEvent>
#include <QDialog>
#include <QPoint>
#include <QWidget>
#include <vector>

#include "xdaqvc/camera.h"


class RecordSettings : public QDialog
{
    Q_OBJECT

public:
    RecordSettings(const std::vector<Camera *> &_cameras, QWidget *parent = nullptr);
    ~RecordSettings() = default;

private:
    std::vector<Camera *> cameras;
    QPoint start_p;

protected:
    void closeEvent(QCloseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
};