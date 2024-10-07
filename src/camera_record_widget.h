#pragma once

#include <fmt/core.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QString>
#include <QWidget>

class DurationLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    DurationLineEdit(QWidget *parent = nullptr) : QLineEdit(parent)
    {
        setPlaceholderText(tr("1ms"));
        setValidator(new QIntValidator(0, 100000, this));
        setFixedWidth(75);
        connect(this, &QLineEdit::editingFinished, [this]() {
            // QFontMetrics fm(text());
            bool ok;
            int value = text().toInt(&ok);
            if (ok) {
                if (value < 1000) {
                    setText(QString::number(value) + "ms");
                } else {
                    double seconds = value / 1000.0;
                    setText(QString::number(seconds, 'f', 3) + "s");
                }
                // int text_width = fontMetrics().horizontalAdvance(text());
                // setFixedWidth(text_width + 30);
            }
        });
    }
    // int get_numeric_value() const
    // {
    //     QString text_value = text();
    //     bool ok;
    //     int result = 0;

    //     if (text_value.endsWith("ms")) {
    //         text_value.chop(2);
    //         result = text_value.toInt(&ok);
    //     } else {
    //         text_value.chop(1);
    //         double seconds = text_value.toDouble(&ok);
    //         result = static_cast<int>(seconds * 1000);
    //     }
    //     return ok ? result : 0;
    // }
};

class CameraRecordWidget : public QWidget
{
    Q_OBJECT
public:
    CameraRecordWidget(QWidget *parent = nullptr);
    ~CameraRecordWidget() = default;
    void set_name(const std::string &_name) { name->setText(QString::fromStdString(_name)); };
    QCheckBox *name;
};