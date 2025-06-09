#include "duration_spinbox.h"

#include <spdlog/spdlog.h>

DurationSpinBox::DurationSpinBox(QWidget *parent) : QSpinBox(parent)
{
    spdlog::info("Creating DurationSpinBox");
    setFixedWidth(75);
    setRange(1, 3600);
    setSingleStep(1);
    setValue(1);
    setSuffix("s");
}