#pragma once

#include <fmt/core.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

// class DurationSpinBox : public QSpinBox
// {
// public:
//     DurationSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
//     {
//         setRange(1, 10000);
//         setValue(1);
//         setSuffix("ms");

//         connect(
//             this, QOverload<int>::of(&QSpinBox::valueChanged), this,
//             &DurationSpinBox::update_suffix
//         );
//     }
// private slots:
//     void update_suffix(int value)
//     {
//         if (value < 1000) {
//             setSuffix("ms");
//         } else {
//             double seconds = value / 1000.0;
//             setValue(seconds);
//             setSuffix("s");
//         }
//     }
// };

class DurationLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    DurationLineEdit(QWidget *parent = nullptr) : QLineEdit(parent)
    {
        setPlaceholderText(tr("1ms"));
        setValidator(new QIntValidator(0, 100000, this));
        setFixedWidth(75);
        // connect(this, &QLineEdit::edi, this, &DurationLineEdit::format_text);
        connect(this, &QLineEdit::editingFinished, this, &DurationLineEdit::format_text);
    }

private slots:
    void format_text()
    {
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
    }
};

class CameraRecordWidget : public QWidget
{
    Q_OBJECT
public:
    CameraRecordWidget(QWidget *parent = nullptr);
    ~CameraRecordWidget() = default;
    void set_name(const std::string &_name) { name->setText(tr(_name.c_str())); };
    QString get_name() { return name->text(); };
    QCheckBox *name;
    QRadioButton *continuous;
    QRadioButton *trigger_on;
    QComboBox *digital_channels;
    QComboBox *trigger_conditions;
    // DurationSpinBox *trigger_duration;
    DurationLineEdit *trigger_duration;
    bool clicked() { return checked; }
private:
    bool checked;
    // private slots:
};