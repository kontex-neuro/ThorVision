#include "no_leading_zero_spinbox.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

NoLeadingZeroSpinBox::NoLeadingZeroSpinBox(QWidget *parent) : QSpinBox(parent)
{
    auto edit = new QLineEdit(this);
    edit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("^[1-9][0-9]{0,3}?$"), edit)
    );
    setLineEdit(edit);
}