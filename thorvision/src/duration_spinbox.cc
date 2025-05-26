#include "duration_spinbox.h"

#include <spdlog/spdlog.h>

namespace
{
auto constexpr HOUR = 3600;
}  // namespace


DurationSpinBox::DurationSpinBox(QWidget *parent) : QSpinBox(parent)
{
    spdlog::info("Creating DurationSpinBox");
    setFixedWidth(75);
    setRange(1, HOUR);
    setSingleStep(1);
    setValue(1);
    setSuffix("s");
}