#include "duration_spinbox.h"

namespace
{
auto constexpr HOUR = 3600;
}  // namespace

DurationSpinBox::DurationSpinBox(QWidget *parent) : QSpinBox(parent)
{
    setFixedWidth(75);
    setRange(1, HOUR);
    setSingleStep(1);
    setValue(1);
    setSuffix("s");
}