
#include "camera_record_widget.h"

#include <spdlog/spdlog.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSettings>

#include "duration_spinbox.h"


namespace
{
auto constexpr CONTINUOUS = "continuous";
auto constexpr TRIGGER_ON = "trigger_on";
auto constexpr DIGITAL_CHANNEL = "digital_channel";
auto constexpr TRIGGER_CONDITION = "trigger_condition";
auto constexpr TRIGGER_DURATION = "trigger_duration";
}  // namespace

CameraRecordWidget::CameraRecordWidget(const std::string &camera_name, QWidget *parent)
    : QWidget(parent)
{
    auto layout = new QHBoxLayout(this);
    auto name = new QLabel(this);
    auto continuous = new QRadioButton(tr("Continuous"), this);
    auto trigger_on = new QRadioButton(tr("Trigger on"), this);
    auto digital_channels = new QComboBox(this);
    auto trigger_conditions = new QComboBox(this);
    spdlog::info("Creating DurationSpinBox.");
    auto trigger_duration = new DurationSpinBox(this);

    trigger_on->setDisabled(true);
    name->setText(QString::fromStdString(camera_name));

    for (auto i = 1; i <= 32; ++i) {
        digital_channels->addItem(QString("DI %1").arg(i));
    }

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

    QSettings settings("KonteX Neuroscience", "Thor Vision");
    settings.beginGroup(name->text());
    auto _continuous = settings.value(CONTINUOUS, true).toBool();
    auto _trigger_on = settings.value(TRIGGER_ON, false).toBool();
    auto _digital_channel = settings.value(DIGITAL_CHANNEL, 0).toUInt();
    auto _trigger_condition = settings.value(TRIGGER_CONDITION, 0).toUInt();
    auto _trigger_duration = settings.value(TRIGGER_DURATION, 1).toUInt();
    settings.setValue(CONTINUOUS, _continuous);
    settings.setValue(TRIGGER_ON, _trigger_on);
    settings.setValue(DIGITAL_CHANNEL, _digital_channel);
    settings.setValue(TRIGGER_CONDITION, _trigger_condition);
    settings.setValue(TRIGGER_DURATION, _trigger_duration);
    settings.endGroup();

    continuous->setChecked(_continuous);
    trigger_on->setChecked(_trigger_on);
    digital_channels->setCurrentIndex(_digital_channel);
    trigger_conditions->setCurrentIndex(_trigger_condition);
    trigger_duration->setValue(_trigger_duration);

    if (continuous->isChecked()) {
        digital_channels->setDisabled(true);
        trigger_conditions->setDisabled(true);
        trigger_duration->setDisabled(true);
    }
    trigger_duration->setDisabled(trigger_conditions->currentIndex() != 2);  // ON For

    connect(
        continuous,
        &QRadioButton::toggled,
        [name, digital_channels, trigger_conditions, trigger_duration](bool checked) {
            spdlog::info(
                "Set camera {} setting '{}' to {}", name->text().toStdString(), CONTINUOUS, checked
            );
            QSettings settings("KonteX Neuroscience", "Thor Vision");
            settings.beginGroup(name->text());
            settings.setValue(CONTINUOUS, checked);
            settings.endGroup();
            digital_channels->setDisabled(checked);
            trigger_conditions->setDisabled(checked);
            trigger_duration->setDisabled(checked);
        }
    );
    connect(trigger_on, &QRadioButton::toggled, [name](bool checked) {
        spdlog::info(
            "Set camera {} setting '{}' to {}", name->text().toStdString(), TRIGGER_ON, checked
        );
        QSettings settings("KonteX Neuroscience", "Thor Vision");
        settings.beginGroup(name->text());
        settings.setValue(TRIGGER_ON, checked);
        settings.endGroup();
    });
    connect(digital_channels, &QComboBox::currentIndexChanged, [name, digital_channels](int index) {
        spdlog::info(
            "Set camera {} setting '{}' to {}",
            name->text().toStdString(),
            DIGITAL_CHANNEL,
            digital_channels->itemText(index).toStdString()
        );
        QSettings settings("KonteX Neuroscience", "Thor Vision");
        settings.beginGroup(name->text());
        settings.setValue(DIGITAL_CHANNEL, index);
        settings.endGroup();
    });
    connect(
        trigger_conditions,
        &QComboBox::currentIndexChanged,
        [name, trigger_conditions, trigger_duration](int index) {
            spdlog::info(
                "Set camera {} setting '{}' to {}",
                name->text().toStdString(),
                TRIGGER_CONDITION,
                trigger_conditions->itemText(index).toStdString()
            );
            QSettings settings("KonteX Neuroscience", "Thor Vision");
            settings.beginGroup(name->text());
            settings.setValue(TRIGGER_CONDITION, index);
            settings.endGroup();

            trigger_duration->setDisabled(index != 2);  // ON For
        }
    );
    connect(trigger_duration, &QSpinBox::valueChanged, [name](int value) {
        spdlog::info(
            "Set camera {} setting '{}' to {}s", name->text().toStdString(), TRIGGER_DURATION, value
        );
        QSettings settings("KonteX Neuroscience", "Thor Vision");
        settings.beginGroup(name->text());
        settings.setValue(TRIGGER_DURATION, value);
        settings.endGroup();
    });
}