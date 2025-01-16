#pragma once

#include <QWidget>

class CameraRecordWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraRecordWidget(const std::string &camera_name, QWidget *parent = nullptr);
    ~CameraRecordWidget() = default;
};