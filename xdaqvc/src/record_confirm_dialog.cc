#include "record_confirm_dialog.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>


namespace
{
auto constexpr DIR_NAME = "dir_name";
auto constexpr SAVE_PATHS = "save_paths";

auto constexpr CONTINUOUS = "continuous";
auto constexpr MAX_SIZE_TIME = "max_size_time";
auto constexpr MAX_FILES = "max_files";
}  // namespace


RecordConfirmDialog::RecordConfirmDialog(const QString &specs, QWidget *parent)
    : QDialog(parent), _dont_ask_again(false)
{
    setWindowTitle("Record Settings Confirm");

    QSettings settings("KonteX Neuroscience", "Thor Vision");
    // settings.beginGroup(name->text());
    auto dir_name = settings.value(DIR_NAME, tr("directory_name")).toString();
    auto save_path = settings.value(SAVE_PATHS).toStringList().first();

    auto continuous = settings.value(CONTINUOUS, true).toBool();
    auto max_size_time = settings.value(MAX_SIZE_TIME, 10).toInt();
    auto max_files = settings.value(MAX_FILES, 10).toInt();
    // settings.endGroup();

    auto layout = new QVBoxLayout(this);
    auto info = new QLabel(
        tr("<b>Are you sure you want to start recording with the following camera "
           "settings?</b><br>"),
        this
    );

    auto camera_specs = new QLabel(specs);
    camera_specs->setWordWrap(true);

    auto record_text = tr("<b>Record Settings:</b><br>"
                          "Save Path: %1<br>"
                          "Record Mode: %2<br>"
                          "File Duration: %3<br>"
                          "Maximum files: %4")
                           .arg(save_path + "/" + dir_name)
                           .arg(continuous ? tr("Continuous") : tr("Split"))
                           .arg(continuous ? tr("N/A") : QString::number(max_size_time) + "min")
                           .arg(continuous ? tr("N/A") : QString::number(max_files));

    auto record_info = new QLabel(record_text, this);
    record_info->setWordWrap(true);

    auto button_widget = new QWidget(this);
    auto button_layout = new QHBoxLayout();
    auto dont_ask_again_checkbox = new QCheckBox(tr("Don't ask me again"), this);
    auto ok = new QPushButton(tr("OK"), this);
    auto cancel = new QPushButton(tr("Cancel"), this);
    ok->setFixedWidth(ok->sizeHint().width());
    cancel->setFixedWidth(cancel->sizeHint().width());

    button_widget->setLayout(button_layout);
    button_layout->addWidget(dont_ask_again_checkbox);
    button_layout->addStretch();
    button_layout->addWidget(ok);
    button_layout->addWidget(cancel);

    layout->addWidget(info);
    layout->addWidget(camera_specs);
    layout->addWidget(record_info);
    layout->addWidget(button_widget);

    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(dont_ask_again_checkbox, &QCheckBox::toggled, [this](bool checked) {
        _dont_ask_again = checked;
    });
}