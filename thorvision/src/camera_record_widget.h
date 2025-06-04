#pragma once

#include <QWidget>
#include <QLabel>

class CameraRecordWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraRecordWidget(const std::string &camera_name, QWidget *parent = nullptr);
    ~CameraRecordWidget() = default;

private:
    QLabel *_name;

public slots:
    void update_camera_name(const QString &new_name);
};