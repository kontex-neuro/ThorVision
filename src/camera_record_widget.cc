
#include "camera_record_widget.h"

#include <QHBoxLayout>
#include <QRadioButton>
#include <QSettings>
#include <QValidator>


CameraRecordWidget::CameraRecordWidget(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    name = new QCheckBox(this);
    QRadioButton *continuous = new QRadioButton(tr("Continuous"), this);
    QRadioButton *trigger_on = new QRadioButton(tr("Trigger on"), this);
    QComboBox *digital_channels = new QComboBox(this);
    QComboBox *trigger_conditions = new QComboBox(this);
    DurationLineEdit *trigger_duration = new DurationLineEdit(this);

    for (int i = 0; i < 32; ++i) {
        digital_channels->addItem(QString("DI %1").arg(i + 1));
    }

    trigger_conditions->addItem(tr("While High"));
    trigger_conditions->addItem(tr("Level"));
    trigger_conditions->addItem(tr("Toggle"));
    trigger_conditions->addItem(tr("ON For"));

    digital_channels->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    trigger_conditions->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    layout->addWidget(name);
    layout->addWidget(continuous);
    layout->addWidget(trigger_on);
    layout->addWidget(digital_channels);
    layout->addWidget(trigger_conditions);
    layout->addWidget(trigger_duration);

    setLayout(layout);

    QSettings settings("KonteX", "VC");
    settings.beginGroup(name->text());
    bool _name = settings.value("name", false).toBool();
    bool _continuous = settings.value("continuous", true).toBool();
    bool _trigger_on = settings.value("trigger_on", false).toBool();
    int _digital_channel = settings.value("digital_channels", 0).toInt();
    int _trigger_condition = settings.value("trigger_conditions", 0).toInt();
    QString _trigger_duration = settings.value("trigger_duration").toString();
    settings.endGroup();

    name->setChecked(_name);
    continuous->setChecked(_continuous);
    trigger_on->setChecked(_trigger_on);
    digital_channels->setCurrentIndex(_digital_channel);
    trigger_conditions->setCurrentIndex(_trigger_condition);
    trigger_duration->setText(_trigger_duration);

    if (continuous->isChecked()) {
        digital_channels->setDisabled(true);
        trigger_conditions->setDisabled(true);
        trigger_duration->setDisabled(true);
    }

    connect(name, &QCheckBox::clicked, [this](bool checked) {
        QSettings settings("KonteX", "VC");
        settings.beginGroup(name->text());
        settings.setValue("name", checked);
        settings.endGroup();
    });
    connect(
        continuous,
        &QRadioButton::toggled,
        [this, digital_channels, trigger_conditions, trigger_duration](bool checked) {
            QSettings settings("KonteX", "VC");
            settings.beginGroup(name->text());
            settings.setValue("continuous", checked);
            settings.endGroup();
            digital_channels->setDisabled(checked);
            trigger_conditions->setDisabled(checked);
            trigger_duration->setDisabled(checked);
        }
    );
    connect(trigger_on, &QRadioButton::toggled, [this](bool checked) {
        QSettings settings("KonteX", "VC");
        settings.beginGroup(name->text());
        settings.setValue("trigger_on", checked);
        settings.endGroup();
    });
    connect(digital_channels, &QComboBox::currentTextChanged, [this](const QString &channel) {
        QSettings settings("KonteX", "VC");
        settings.beginGroup(name->text());
        settings.setValue("digital_channels", channel);
        settings.endGroup();
    });
    connect(trigger_conditions, &QComboBox::currentTextChanged, [this](const QString &condition) {
        QSettings settings("KonteX", "VC");
        settings.beginGroup(name->text());
        settings.setValue("trigger_conditions", condition);
        settings.endGroup();
    });
    connect(trigger_duration, &QLineEdit::textEdited, [this](const QString &duration) {
        QSettings settings("KonteX", "VC");
        settings.beginGroup(name->text());
        settings.setValue("trigger_duration", duration.toInt());
        fmt::println("trigger_duration = {}", duration.toInt());
        settings.endGroup();
    });
}