#include "record_settings.h"

#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QCheckBox>
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
#include <QSpinBox>

#include "camera_record_widget.h"
#include "dir_name_combobox.h"
#include "save_paths_combobox.h"


namespace
{
auto constexpr CONTINUOUS = "continuous";
auto constexpr SPLIT_RECORD = "split_record";
auto constexpr MAX_SIZE_TIME = "max_size_time";
auto constexpr MAX_FILES = "max_files";

auto constexpr ADDITIONAL_METADATA = "additional_metadata";

auto constexpr SAVE_PATHS = "save_paths";
}  // namespace

RecordSettings::RecordSettings(const std::vector<Camera *> &_cameras, QWidget *parent)
    : QDialog(parent)
{
    setMinimumSize(690, 360);
    setMaximumSize(690, 360);
    resize(690, 360);
    cameras = _cameras;

    auto title = new QLabel(tr("REC Settings"));
    QFont title_font;
    title_font.setPointSize(12);
    title_font.setBold(true);
    title->setFont(title_font);

    setWindowTitle(" ");
    setWindowIcon(QIcon());

    auto cameras_list = new QListWidget(this);
    auto layout = new QGridLayout(this);

    for (auto camera : cameras) {
        auto item = new QListWidgetItem(cameras_list);
        auto widget = new CameraRecordWidget(this, camera->name());
        item->setSizeHint(widget->sizeHint());
        cameras_list->setItemWidget(item, widget);
    }

    auto continuous = new QRadioButton(tr("Continuous"), this);
    auto split_record = new QRadioButton(tr("Split record into"), this);
    auto max_size_time = new QSpinBox(this);
    auto max_files_text = new QLabel(tr("Max files"), this);
    auto max_files = new QSpinBox(this);
    auto additional_metadata = new QCheckBox(tr("Extract metadata in seperate files"), this);
    max_size_time->setFixedWidth(100);
    max_size_time->setRange(1, 60);
    max_size_time->setSuffix("m");
    max_files->setRange(1, 60);

    auto record_mode_widget = new QWidget(this);
    auto record_mode_layout = new QHBoxLayout;
    record_mode_widget->setLayout(record_mode_layout);
    record_mode_widget->setLayout(record_mode_layout);
    record_mode_layout->addWidget(continuous);
    record_mode_layout->addWidget(split_record);
    record_mode_layout->addWidget(max_size_time);
    record_mode_layout->addWidget(max_files_text);
    record_mode_layout->addWidget(max_files);

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

    layout->addWidget(title, 0, 0);
    layout->addWidget(cameras_list, 1, 0);
    layout->addWidget(file_settings_widget, 2, 0);
    setLayout(layout);

    QSettings settings("KonteX", "ThorVision");
    auto _continuous = settings.value(CONTINUOUS, true).toBool();
    auto _split_record = settings.value(SPLIT_RECORD, false).toBool();
    auto _max_size_time = settings.value(MAX_SIZE_TIME, 10).toInt();
    auto _max_files = settings.value(MAX_FILES, 10).toInt();
    auto _additional_metadata = settings.value(ADDITIONAL_METADATA, false).toBool();
    settings.setValue(CONTINUOUS, _continuous);
    settings.setValue(SPLIT_RECORD, _split_record);
    settings.setValue(MAX_SIZE_TIME, _max_size_time);
    settings.setValue(ADDITIONAL_METADATA, _additional_metadata);
    settings.setValue(MAX_FILES, _max_files);

    continuous->setChecked(_continuous);
    split_record->setChecked(_split_record);
    max_size_time->setValue(_max_size_time);
    max_files->setValue(_max_files);
    additional_metadata->setChecked(_additional_metadata);

    max_size_time->setDisabled(continuous->isChecked());
    max_files->setDisabled(continuous->isChecked());

    connect(split_record, &QRadioButton::toggled, this, [max_size_time, max_files](bool checked) {
        QSettings settings("KonteX", "ThorVision");
        settings.setValue(CONTINUOUS, !checked);
        settings.setValue(SPLIT_RECORD, checked);
        max_size_time->setDisabled(!checked);
        max_files->setDisabled(!checked);
    });
    connect(max_size_time, &QSpinBox::valueChanged, this, [](int seconds) {
        QSettings("KonteX", "ThorVision").setValue(MAX_SIZE_TIME, seconds);
    });
    connect(max_files, &QSpinBox::valueChanged, this, [](int files) {
        QSettings("KonteX", "ThorVision").setValue(MAX_FILES, files);
    });
    connect(select_save_path, &QPushButton::clicked, [this, save_paths]() {
        auto path = QFileDialog::getExistingDirectory(this);
        if (!path.isEmpty()) {
            auto path_index = save_paths->findText(path);
            if (path_index != -1) {
                save_paths->removeItem(path_index);
            }
            save_paths->insertItem(0, path);
            save_paths->setCurrentIndex(0);
            auto paths = QStringList();
            for (int i = 0; i < save_paths->count(); ++i) {
                paths << save_paths->itemText(i);
            }
            QSettings("KonteX", "ThorVision").setValue(SAVE_PATHS, path);
        }
    });
    connect(additional_metadata, &QCheckBox::clicked, [](bool checked) {
        QSettings("KonteX", "ThorVision").setValue(ADDITIONAL_METADATA, checked);
        // TODO
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