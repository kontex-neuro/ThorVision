#pragma once

#include <QLabel>
#include <QSpinBox>
#include <QString>
#include <QWidget>

class DurationSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    DurationSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
    {
        const int SEC = 60;
        setFixedWidth(75);
        setRange(1, 1000 * SEC);
        setSingleStep(1);
        setValue(1);
        setSuffix("ms");
    }
};

class CameraRecordWidget : public QWidget
{
    Q_OBJECT
public:
    CameraRecordWidget(QWidget *parent = nullptr, const std::string &camera_name = "");
    ~CameraRecordWidget() = default;
};