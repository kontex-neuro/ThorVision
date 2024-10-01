
#include "camera_record_widget.h"

#include <QHBoxLayout>
#include <QValidator>
#include <QSettings>


CameraRecordWidget::CameraRecordWidget(QWidget *parent) : QWidget(parent), checked(false)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    name = new QCheckBox(this);
    continuous = new QRadioButton(tr("Continuous"), this);
    trigger_on = new QRadioButton(tr("Trigger on"), this);
    digital_channels = new QComboBox(this);
    trigger_conditions = new QComboBox(this);
    trigger_duration = new DurationLineEdit(this);
    // trigger_duration = new QLineEdit(this);
    // trigger_duration = new DurationSpinBox(this);

    for (int i = 0; i < 32; ++i) {
        digital_channels->addItem(QString("DI %1").arg(i + 1));
    }

    trigger_conditions->addItem(tr("While High"));
    trigger_conditions->addItem(tr("Level"));
    trigger_conditions->addItem(tr("Toggle"));
    trigger_conditions->addItem(tr("ON For"));

    continuous->setChecked(true);
    digital_channels->setCurrentIndex(0);
    trigger_conditions->setCurrentIndex(0);

    digital_channels->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    trigger_conditions->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    layout->addWidget(name);
    layout->addWidget(continuous);
    layout->addWidget(trigger_on);
    layout->addWidget(digital_channels);
    layout->addWidget(trigger_conditions);
    layout->addWidget(trigger_duration);

    setLayout(layout);

    connect(name, &QCheckBox::clicked, [this](bool _checked){
        checked = _checked;
    });
}