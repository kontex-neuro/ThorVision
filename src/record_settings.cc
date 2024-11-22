#include "record_settings.h"

#include <qnamespace.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDrag>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>

#include "camera_record_widget.h"
#include "dir_name_combobox.h"
#include "save_paths_combobox.h"

namespace
{
constexpr auto CONTINUOUS = "continuous";
constexpr auto SPLIT_RECORD = "split_record";
constexpr auto SAVE_PATHS = "save_paths";
constexpr auto ADDITIONAL_METADATA = "additional_metadata";
constexpr auto RECORD_SECONDS = "record_seconds";
}  // namespace

RecordSettings::RecordSettings(const std::vector<Camera *> &_cameras, QWidget *parent)
    : QDialog(parent)
{
    setMaximumSize(690, 360);
    resize(690, 360);
    cameras = _cameras;
    auto title = new QLabel(tr("REC Settings"));
    QFont title_font;
    title_font.setPointSize(12);
    title_font.setBold(true);
    title->setFont(title_font);

    // setWindowFlags(Qt::FramelessWindowHint);
    // QRegion maskedRegion(
    //     width() / 2 - side / 2, height() / 2 - side / 2, side, side, QRegion::Ellipse
    // );
    // setMask(maskedRegion);
    // setStyleSheet("RecordSettings{border-radius: 10px;}");

    // setWindowFlags(Qt::Popup);

    auto cameras_list = new QListWidget(this);
    auto layout = new QGridLayout(this);

    for (auto camera : cameras) {
        auto item = new QListWidgetItem(cameras_list);
        auto camera_record_widget = new CameraRecordWidget(this, camera->get_name());
        item->setSizeHint(camera_record_widget->sizeHint());
        cameras_list->setItemWidget(item, camera_record_widget);
    }

    auto continuous = new QRadioButton(tr("Continuous"), this);
    auto split_record = new QRadioButton(tr("Split camera record into"), this);
    auto record_seconds = new QSpinBox(this);
    auto additional_metadata = new QCheckBox(tr("Extract metadata in seperate files"), this);
    record_seconds->setFixedWidth(100);
    record_seconds->setRange(1, 3600);
    record_seconds->setSuffix("s");

    auto record_mode_widget = new QWidget(this);
    auto record_mode_layout = new QHBoxLayout;
    record_mode_widget->setLayout(record_mode_layout);
    record_mode_layout->addWidget(continuous);
    record_mode_layout->addWidget(split_record);
    record_mode_layout->addWidget(record_seconds);

    auto file_location_widget = new QWidget(this);
    auto file_location_layout = new QHBoxLayout;
    auto save_paths = new SavePathsComboBox(this);
    auto select_save_path = new QPushButton(tr("..."), this);
    auto dir_name = new DirNameComboBox(this);
    select_save_path->setFixedWidth(30);
    file_location_widget->setLayout(file_location_layout);
    file_location_layout->addWidget(save_paths);
    file_location_layout->addWidget(select_save_path);
    file_location_layout->addWidget(dir_name);

    auto file_settings_widget = new QWidget(this);
    auto file_settings_layout = new QGridLayout;
    file_settings_widget->setLayout(file_settings_layout);
    file_settings_layout->addWidget(record_mode_widget, 0, 0, Qt::AlignLeft);
    file_settings_layout->addWidget(additional_metadata, 0, 1, Qt::AlignRight);
    file_settings_layout->addWidget(file_location_widget, 1, 0, 1, 2, Qt::AlignCenter);
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addWidget(title, 0, 0);
    layout->addWidget(cameras_list, 1, 0);
    layout->addWidget(file_settings_widget, 2, 0);
    layout->addWidget(buttonBox, 3, 0);
    setLayout(layout);

    QSettings settings("KonteX", "VC");
    auto _continuous = settings.value(CONTINUOUS, true).toBool();
    auto _split_record = settings.value(SPLIT_RECORD, false).toBool();
    auto _record_seconds = settings.value(RECORD_SECONDS, 10).toInt();
    auto _additional_metadata = settings.value(ADDITIONAL_METADATA, false).toBool();
    settings.setValue(CONTINUOUS, _continuous);
    settings.setValue(SPLIT_RECORD, _split_record);
    settings.setValue(RECORD_SECONDS, _record_seconds);
    settings.setValue(ADDITIONAL_METADATA, _additional_metadata);

    continuous->setChecked(_continuous);
    split_record->setChecked(_split_record);
    record_seconds->setValue(_record_seconds);
    additional_metadata->setChecked(_additional_metadata);
    record_seconds->setDisabled(continuous->isChecked());

    connect(split_record, &QRadioButton::toggled, this, [record_seconds](bool checked) {
        QSettings("KonteX", "VC").setValue(SPLIT_RECORD, checked);
        record_seconds->setDisabled(!checked);
    });
    connect(record_seconds, &QSpinBox::valueChanged, this, [](int seconds) {
        QSettings("KonteX", "VC").setValue(RECORD_SECONDS, seconds);
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(select_save_path, &QPushButton::clicked, [this, save_paths]() {
        auto path = QFileDialog::getExistingDirectory(this);
        if (!path.isEmpty()) {
            save_paths->insertItem(0, path);
            save_paths->setCurrentIndex(0);
            auto paths = QStringList();
            for (int i = 0; i < save_paths->count(); ++i) {
                paths << save_paths->itemText(i);
            }
            QSettings("KonteX", "VC").setValue(SAVE_PATHS, path);
        }
    });
    connect(additional_metadata, &QCheckBox::clicked, [](bool checked) {
        QSettings("KonteX", "VC").setValue(ADDITIONAL_METADATA, checked);
    });
}

void RecordSettings::closeEvent(QCloseEvent *e) { e->accept(); }

void RecordSettings::mousePressEvent(QMouseEvent *e)
{
    start_p = e->globalPosition().toPoint() - frameGeometry().topLeft();
    e->accept();
}

void RecordSettings::mouseMoveEvent(QMouseEvent *e)
{
    move(e->globalPosition().toPoint() - start_p);
    e->accept();
}