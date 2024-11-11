#pragma once

#include <QSpinBox>


class DurationSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    DurationSpinBox(QWidget *parent = nullptr);
    ~DurationSpinBox() = default;
};