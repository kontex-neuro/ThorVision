#pragma once

#include <QWidget>

class CameraRecordWidget : public QWidget
{
    Q_OBJECT
public:
    CameraRecordWidget(QWidget *parent = nullptr, const std::string &camera_name = "");
    ~CameraRecordWidget() = default;
};