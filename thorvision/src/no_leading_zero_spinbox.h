#pragma once

#include <QSpinBox>
#include <QWidget>

class NoLeadingZeroSpinBox : public QSpinBox
{
public:
    NoLeadingZeroSpinBox(QWidget *parent = nullptr);
    ~NoLeadingZeroSpinBox() = default;
};
