#pragma once

#include <QSpinBox>


class DurationSpinBox : public QSpinBox
{
    Q_OBJECT
    
public:
    explicit DurationSpinBox(QWidget *parent = nullptr);
    ~DurationSpinBox() = default;
};