#include "record_settings.h"

#include <fmt/core.h>
#include <qnamespace.h>

#include <QDialogButtonBox>
#include <QDrag>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
    // setMinimumSize(600, 300);
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

    for (auto camera : cameras) {
        QListWidgetItem *item = new QListWidgetItem(cameras_list);
        CameraRecordWidget *camera_item_widget = new CameraRecordWidget(this);
        camera_item_widget->set_name(camera->get_name());
        item->setSizeHint(camera_item_widget->sizeHint());
        cameras_list->setItemWidget(item, camera_item_widget);
    }

    QRadioButton *continuous = new QRadioButton(tr("Continuous"), this);
    QRadioButton *split_record = new QRadioButton(tr("Split camera record into"), this);
    QSpinBox *record_seconds = new QSpinBox(this);
    continuous->setChecked(true);
    record_seconds->setFixedWidth(100);
    record_seconds->setRange(1, 3600);
    record_seconds->setSuffix("s");
    record_seconds->setDisabled(true);

    additional_metadata = new QCheckBox(tr("Extract metadata in seperate files"), this);

    QWidget *record_mode = new QWidget(this);
    QHBoxLayout *record_mode_layout = new QHBoxLayout;
    record_mode->setLayout(record_mode_layout);
    record_mode_layout->addWidget(continuous);
    record_mode_layout->addWidget(split_record);
    record_mode_layout->addWidget(record_seconds);

    QWidget *file_location_widget = new QWidget(this);
    QHBoxLayout *file_location_layout = new QHBoxLayout;
    file_location_widget->setLayout(file_location_layout);

    save_path = new QComboBox(this);
    QPushButton *select_save_path = new QPushButton(tr("..."), this);
    dir_name = new QComboBox(this);

    save_path->addItem(QDir::currentPath());
    save_path->setCurrentIndex(0);
    save_path->setEditable(true);
    save_path->setFixedWidth(300);
    select_save_path->setFixedWidth(30);

    dir_name->addItem(tr("YYYY-MM-DD_HH-MM-SS"));
    dir_name->addItem(tr("directory_name"));

    file_location_layout->addWidget(save_path);
    file_location_layout->addWidget(select_save_path);
    file_location_layout->addWidget(dir_name);

    QWidget *file_settings_widget = new QWidget(this);
    QGridLayout *file_settings_layout = new QGridLayout;
    file_settings_widget->setLayout(file_settings_layout);
    file_settings_layout->addWidget(record_mode, 0, 0, Qt::AlignLeft);
    file_settings_layout->addWidget(additional_metadata, 0, 1, Qt::AlignRight);
    file_settings_layout->addWidget(file_location_widget, 1, 0);
    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addWidget(title, 0, 0);
    layout->addWidget(cameras_list, 1, 0);
    layout->addWidget(file_settings_widget, 2, 0);
    layout->addWidget(buttonBox, 3, 0);

    // save_path->setText(filename);
    // // QFontMetrics metrics(save_path->font());
    // int text_width = save_path->fontMetrics().horizontalAdvance(save_path->text()) + 10;
    // save_path->setFixedWidth(text_width);

    setLayout(layout);

    QSettings settings("KonteX", "VC");
    QString _save_path = settings.value("save_path").toString();
    QString _dir_name = settings.value("dir_name").toString();
    bool _additional_metadata = settings.value("additional_metadata").toBool();
    bool _continuous = settings.value("continuous").toBool();
    bool _split_record = settings.value("split_record").toBool();
    int _record_seconds = settings.value("record_seconds").toInt();

    continuous->setChecked(_continuous);
    split_record->setChecked(_split_record);
    record_seconds->setValue(_record_seconds);
    save_path->setCurrentText(_save_path);
    dir_name->setCurrentText(_dir_name);
    additional_metadata->setChecked(_additional_metadata);

    record_seconds->setDisabled(continuous->isChecked());
    dir_name->setEditable(dir_name->currentIndex() != 0);

    connect(split_record, &QRadioButton::toggled, this, [record_seconds](bool checked) {
        QSettings("KonteX", "VC").setValue("split_record", checked);
        record_seconds->setDisabled(!checked);
    });
    connect(record_seconds, &QSpinBox::valueChanged, this, [](int seconds) {
        QSettings("KonteX", "VC").setValue("record_seconds", seconds);
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(select_save_path, &QPushButton::clicked, [this]() {
        QString selected_file_path = QFileDialog::getExistingDirectory(this);
        save_path->setCurrentText(selected_file_path);
        QSettings("KonteX", "VC").setValue("save_path", selected_file_path);
    });
    connect(dir_name, &QComboBox::currentIndexChanged, [this](int index) {
        dir_name->setEditable(index != 0);
        auto dir_date = index == 0;
        fmt::println("dir_date = {}", dir_date);
        QSettings("KonteX", "VC").setValue("dir_date", dir_date);
    });
    connect(dir_name, &QComboBox::editTextChanged, [](const QString &_dir_name) {
        // TODO: Add validator to dir_name
        // TODO: editTextChanged signal is not suitable, use QLineEdit textEdited signal
        QSettings("KonteX", "VC").setValue("dir_name", _dir_name);
    });
    connect(additional_metadata, &QCheckBox::clicked, [](bool checked) {
        QSettings("KonteX", "VC").setValue("additional_metadata", checked);
    });
}

void RecordSettings::closeEvent(QCloseEvent *e) { e->accept(); }

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