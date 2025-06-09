#include "record_settings.h"

#include <spdlog/spdlog.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>

#include "camera_record_widget.h"
#include "no_leading_zero_spinbox.h"


namespace
{
auto constexpr CONTINUOUS = "continuous";
auto constexpr SPLIT_RECORD = "split_record";
auto constexpr MAX_SIZE_TIME = "max_size_time";
auto constexpr TIME_UNIT = "time_unit";
auto constexpr MAX_FILES = "max_files";

auto constexpr OPEN_VIDEO_FOLDER = "open_video_folder";

auto constexpr SAVE_PATHS = "save_paths";
}  // namespace

RecordSettings::RecordSettings(QWidget *parent) : QDialog(parent)
{
    spdlog::info("Creating RecordSettings");

    auto title = new QLabel(tr("REC Settings"), this);
    QFont title_font;
    title_font.setPointSize(12);
    title_font.setBold(true);
    title->setFont(title_font);

    auto layout = new QGridLayout(this);
    _camera_list = new QListWidget(this);

    auto continuous = new QRadioButton(tr("Continuous"), this);
    auto split_record = new QRadioButton(tr("Split record into"), this);
    auto max_size_time = new NoLeadingZeroSpinBox(this);
    auto max_files_text = new QLabel(tr("Max files"), this);
    auto time_unit = new QComboBox(this);
    auto max_files = new NoLeadingZeroSpinBox(this);
    auto open_video_folder = new QCheckBox(tr("Open video folder after recording"), this);

    max_size_time->setFixedWidth(60);
    max_size_time->setRange(1, 9999);

    time_unit->addItem(tr("seconds"));
    time_unit->addItem(tr("minutes"));
    time_unit->addItem(tr("hours"));
    time_unit->addItem(tr("days"));

    max_files->setFixedWidth(60);
    // TODO: minimum should be 1, libxvc will not allow 1 file, it will crash
    max_files->setRange(2, 9999);

    auto record_mode_widget = new QWidget(this);
    auto record_mode_layout = new QHBoxLayout(record_mode_widget);
    record_mode_layout->addWidget(continuous);
    record_mode_layout->addWidget(split_record);
    record_mode_layout->addWidget(max_size_time);
    record_mode_layout->addWidget(time_unit);
    record_mode_layout->addWidget(max_files_text);
    record_mode_layout->addWidget(max_files);

    auto file_location_widget = new QWidget(this);
    auto file_location_layout = new QHBoxLayout(file_location_widget);

    _save_paths = new SavePathsComboBox(this);
    auto select_save_path = new QPushButton(tr("..."), this);
    _dir_name = new DirNameComboBox(this);
    select_save_path->setFixedWidth(30);
    file_location_layout->addWidget(_save_paths);
    file_location_layout->addWidget(select_save_path);
    file_location_layout->addWidget(_dir_name);

    auto file_settings_widget = new QWidget(this);
    auto file_settings_layout = new QGridLayout(file_settings_widget);
    file_settings_layout->addWidget(record_mode_widget, 1, 0, Qt::AlignLeft);
    file_settings_layout->addWidget(open_video_folder, 1, 1, Qt::AlignRight);
    file_settings_layout->addWidget(file_location_widget, 0, 0, 1, 2, Qt::AlignCenter);

    layout->addWidget(title, 0, 0);
    layout->addWidget(_camera_list, 1, 0);
    layout->addWidget(file_settings_widget, 2, 0);

    QSettings settings("KonteX Neuroscience", "ThorVision");
    auto _continuous = settings.value(CONTINUOUS, true).toBool();
    auto _split_record = settings.value(SPLIT_RECORD, false).toBool();
    auto _max_size_time = settings.value(MAX_SIZE_TIME, 10).toInt();
    auto _time_unit = settings.value(TIME_UNIT, 0).toInt();
    auto _max_files = settings.value(MAX_FILES, 10).toInt();
    auto _open_video_folder = settings.value(OPEN_VIDEO_FOLDER, true).toBool();
    settings.setValue(CONTINUOUS, _continuous);
    settings.setValue(SPLIT_RECORD, _split_record);
    settings.setValue(MAX_SIZE_TIME, _max_size_time);
    settings.setValue(TIME_UNIT, _time_unit);
    settings.setValue(MAX_FILES, _max_files);

    continuous->setChecked(_continuous);
    split_record->setChecked(_split_record);
    max_size_time->setValue(_max_size_time);
    time_unit->setCurrentIndex(_time_unit);
    max_files->setValue(_max_files);
    open_video_folder->setChecked(_open_video_folder);

    max_size_time->setDisabled(continuous->isChecked());
    time_unit->setDisabled(continuous->isChecked());
    max_files->setDisabled(continuous->isChecked());

    connect(
        split_record,
        &QRadioButton::toggled,
        this,
        [max_size_time, time_unit, max_files](bool checked) {
            spdlog::info("RadioButton 'split_record' selected option: {}", checked);
            QSettings settings("KonteX Neuroscience", "ThorVision");
            settings.setValue(CONTINUOUS, !checked);
            settings.setValue(SPLIT_RECORD, checked);
            max_size_time->setDisabled(!checked);
            time_unit->setDisabled(!checked);
            max_files->setDisabled(!checked);
        }
    );
    connect(max_size_time, &QSpinBox::valueChanged, this, [](int time) {
        spdlog::info("SpinBox 'max_size_time' selected time: {}", time);
        QSettings("KonteX Neuroscience", "ThorVision").setValue(MAX_SIZE_TIME, time);
    });
    connect(time_unit, &QComboBox::currentIndexChanged, this, [time_unit](int index) {
        spdlog::info(
            "SpinBox 'time_unit' selected time unit: {}", time_unit->itemText(index).toStdString()
        );
        QSettings("KonteX Neuroscience", "ThorVision").setValue(TIME_UNIT, index);
    });
    connect(max_files, &QSpinBox::valueChanged, this, [](int files) {
        spdlog::info("SpinBox 'max_files' selected file: {}", files);
        QSettings("KonteX Neuroscience", "ThorVision").setValue(MAX_FILES, files);
    });
    connect(select_save_path, &QPushButton::clicked, [this]() {
        auto path = QFileDialog::getExistingDirectory(this);
        spdlog::info("PushButton 'select_save_path' selected path: {}", path.toStdString());

        if (!_save_paths->valid_path(path)) {
            QMessageBox::warning(
                this,
                tr("Invalid Save Path"),
                tr("The save path you selected is not valid.\n"
                   "It has been reset to the default location:\n%1")
                    .arg(QString::fromStdString(_save_paths->default_save_path()))
            );
            _save_paths->reset_path(QString::fromStdString(_save_paths->default_save_path()));
            return;
        }
        _save_paths->reset_path(path);
    });
    connect(open_video_folder, &QCheckBox::clicked, this, [](bool checked) {
        spdlog::info("CheckBox 'open_video_folder' selected option: {}", checked);
        QSettings("KonteX Neuroscience", "ThorVision").setValue(OPEN_VIDEO_FOLDER, checked);
    });
}

void RecordSettings::add_camera(Camera *camera)
{
    auto id = camera->id();
    auto item = new QListWidgetItem(_camera_list);
    auto widget = new CameraRecordWidget(camera->name(), this);

    item->setData(Qt::UserRole, id);
    item->setSizeHint(widget->sizeHint());

    _camera_list->setItemWidget(item, widget);
    _camera_item_map[id] = item;
    _camera_widget_map[id] = widget;
}

void RecordSettings::remove_camera(int const id)
{
    if (_camera_item_map.contains(id)) {
        auto item = _camera_item_map[id];
        _camera_item_map.erase(id);
        delete _camera_list->takeItem(_camera_list->row(item));

        _camera_widget_map.erase(id);
    }
}

void RecordSettings::closeEvent(QCloseEvent *e)
{
    spdlog::info("Closing RecordSettings");

    if (!_dir_name->valid_current_text()) {
        QMessageBox::warning(
            this,
            tr("Invalid Directory Name"),
            tr("The directory name you entered is not valid.\n"
               "It has been reset to the default: \"%1\".")
                .arg(QString::fromStdString(_dir_name->default_dir_name()))
        );
        e->ignore();
        return;
    }

    if (!_save_paths->valid_current_text()) {
        QMessageBox::warning(
            this,
            tr("Invalid Save Path"),
            tr("The save path you entered is not valid.\n"
               "It has been reset to the default location:\n%1")
                .arg(QString::fromStdString(_save_paths->default_save_path()))
        );
        e->ignore();
        return;
    }

    e->accept();
}