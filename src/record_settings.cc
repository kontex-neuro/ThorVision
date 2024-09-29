#include "record_settings.h"

#include <fmt/core.h>
#include <qnamespace.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDrag>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <nlohmann/json.hpp>

#include "camera_record_widget.h"


using nlohmann::json;

RecordSettings::RecordSettings(const std::vector<Camera *> &_cameras, QWidget *parent)
    : QDialog(parent)
{
    resize(600, 300);
    cameras = _cameras;
    QLabel *title = new QLabel(tr("REC Settings"));
    QFont title_font("Arial", 12, QFont::Bold);
    title->setFont(title_font);

    // setWindowFlags(Qt::FramelessWindowHint);
    // QRegion maskedRegion(
    //     width() / 2 - side / 2, height() / 2 - side / 2, side, side, QRegion::Ellipse
    // );
    // setMask(maskedRegion);
    // setStyleSheet("RecordSettings{border-radius: 10px;}");

    // setWindowFlags(Qt::Popup);

    cameras_list = new QListWidget(this);
    QGridLayout *layout = new QGridLayout(this);

    load_cameras();

    QRadioButton *continuous = new QRadioButton(tr("Continuous"), this);
    QRadioButton *split_record = new QRadioButton(tr("Split camera record into"), this);
    QSpinBox *seconds = new QSpinBox(this);
    seconds->setFixedWidth(100);
    seconds->setRange(1, 3600);
    seconds->setSuffix("s");

    QCheckBox *extract_metadata = new QCheckBox(tr("Extract metadata in seperate files"), this);

    QWidget *record_mode = new QWidget(this);
    QHBoxLayout *record_mode_layout = new QHBoxLayout;
    record_mode->setLayout(record_mode_layout);
    record_mode_layout->addWidget(continuous);
    record_mode_layout->addWidget(split_record);
    record_mode_layout->addWidget(seconds);

    QWidget *file_location_widget = new QWidget(this);
    QHBoxLayout *file_location_layout = new QHBoxLayout;
    file_location_widget->setLayout(file_location_layout);

    QComboBox *save_path = new QComboBox(this);
    QPushButton *select_save_path = new QPushButton(tr("..."), this);
    QComboBox *dir_name = new QComboBox(this);

    save_path->addItem(QDir::currentPath());
    save_path->setCurrentIndex(0);
    save_path->setEditable(true);
    save_path->setFixedWidth(300);
    select_save_path->setFixedWidth(30);

    dir_name->addItem(tr("YYYY-MM-DD_HH-MM-SS"));
    dir_name->addItem(tr("directory_name"));
    dir_name->setCurrentIndex(0);

    file_location_layout->addWidget(save_path);
    file_location_layout->addWidget(select_save_path);
    file_location_layout->addWidget(dir_name);


    QWidget *file_settings_widget = new QWidget(this);
    QGridLayout *file_settings_layout = new QGridLayout;
    file_settings_widget->setLayout(file_settings_layout);
    file_settings_layout->addWidget(record_mode, 0, 0, Qt::AlignLeft);
    file_settings_layout->addWidget(extract_metadata, 0, 1, Qt::AlignRight);
    file_settings_layout->addWidget(file_location_widget, 1, 0);
    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);


    layout->addWidget(title, 0, 0);
    layout->addWidget(cameras_list, 1, 0);
    layout->addWidget(file_settings_widget, 2, 0);
    layout->addWidget(buttonBox, 3, 0);

    // QString filename = QFileDialog::getExistingDirectory(this, tr("Select Existing Directory"));
    // settings.setValue("record_directory", filename);

    // save_path->setText(filename);
    // // QFontMetrics metrics(save_path->font());
    // int text_width = save_path->fontMetrics().horizontalAdvance(save_path->text()) + 10;
    // save_path->setFixedWidth(text_width);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &RecordSettings::on_ok_clicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(select_save_path, &QPushButton::clicked, [this, save_path]() {
        QString selected_file_path = QFileDialog::getExistingDirectory(this);
        save_path->setCurrentText(selected_file_path);
        QSettings("KonteX", "VC").setValue("save_path", selected_file_path);
    });
    connect(dir_name, &QComboBox::currentIndexChanged, this, [dir_name]() {
        if (dir_name->currentIndex() == 0) {
            dir_name->setEditable(false);
            QSettings("KonteX", "VC")
                .setValue("dir_name", QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
        } else {
            QSettings("KonteX", "VC").setValue("dir_name", dir_name->currentText());
        }
    });
    connect(extract_metadata, &QCheckBox::clicked, [this](bool checked) {
        QSettings("KonteX", "VC").setValue("extract_metadata", checked);
    });


    setLayout(layout);

    load_settings();
}

void RecordSettings::load_cameras()
{
    for (auto camera : cameras) {
        QListWidgetItem *item = new QListWidgetItem(cameras_list);
        CameraRecordWidget *camera_item_widget = new CameraRecordWidget(this);
        camera_item_widget->set_name(camera->get_name());
        camera_record_widgets.push_back(camera_item_widget);
        item->setSizeHint(camera_item_widget->sizeHint());

        cameras_list->setItemWidget(item, camera_item_widget);
    }
}

void RecordSettings::closeEvent(QCloseEvent *e) { e->accept(); }

void RecordSettings::on_ok_clicked()
{
    QSettings settings("KonteX", "VC");

    for (auto widget : camera_record_widgets) {
        settings.beginGroup(widget->get_name());
        settings.setValue("name", widget->name->isChecked());
        settings.setValue("continuous", widget->continuous->isChecked());
        settings.setValue("trigger_on", widget->trigger_on->isChecked());
        settings.setValue("digital_channels", widget->digital_channels->currentIndex());
        settings.setValue("trigger_conditions", widget->trigger_conditions->currentIndex());
        settings.setValue("trigger_duration", widget->trigger_duration->text());
        settings.endGroup();
    }
    accept();
}

void RecordSettings::load_settings()
{
    QSettings settings("KonteX", "VC");

    for (auto widget : camera_record_widgets) {
        settings.beginGroup(widget->get_name());
        bool name = settings.value("name", widget->name->isChecked()).toBool();
        bool continuous = settings.value("continuous", widget->continuous->isChecked()).toBool();
        bool trigger_on = settings.value("trigger_on", widget->trigger_on->isChecked()).toBool();
        int digital_channel_index =
            settings.value("digital_channels", widget->digital_channels->currentIndex()).toInt();
        int trigger_condition_index =
            settings.value("trigger_conditions", widget->trigger_conditions->currentIndex())
                .toInt();
        QString trigger_duration =
            settings.value("trigger_duration", widget->trigger_duration->text()).toString();
        settings.endGroup();

        widget->name->setChecked(name);
        widget->continuous->setChecked(continuous);
        widget->trigger_on->setChecked(trigger_on);
        widget->digital_channels->setCurrentIndex(digital_channel_index);
        widget->trigger_conditions->setCurrentIndex(trigger_condition_index);
        widget->trigger_duration->setText(trigger_duration);
    }
}

void RecordSettings::mousePressEvent(QMouseEvent *e)
{
    start_p = e->globalPosition().toPoint() - this->frameGeometry().topLeft();
    e->accept();
}

void RecordSettings::mouseMoveEvent(QMouseEvent *e)
{
    move(e->globalPosition().toPoint() - start_p);
    e->accept();
}