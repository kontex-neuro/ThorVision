
#include "camera_record_widget.h"

#include <QHBoxLayout>
#include <QValidator>



CameraRecordWidget::CameraRecordWidget(QWidget *parent) : QWidget(parent)
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
    // trigger_duration->setText(tr("1ms"));
    // trigger_duration->setValue(1);
    // trigger_duration->setSuffix("ms");
    // connect(this, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomSpinBox::updateSuffix);

    // trigger_duration->setValidator(new QIntValidator(0, 100, this));

    digital_channels->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    trigger_conditions->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    layout->addWidget(name);
    layout->addWidget(continuous);
    layout->addWidget(trigger_on);
    layout->addWidget(digital_channels);
    layout->addWidget(trigger_conditions);
    layout->addWidget(trigger_duration);

    setLayout(layout);
}

void CameraRecordWidget::set_name(std::string _name) { name->setText(tr(_name.c_str())); }
QString CameraRecordWidget::get_name() { return name->text(); }