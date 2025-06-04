#include "dir_name_combobox.h"

#include <spdlog/spdlog.h>

#include <QLineEdit>
#include <QSettings>
#include <QTimer>

namespace
{
auto constexpr DIR_NAME = "dir_name";
auto constexpr DIR_DATE = "dir_date";
}  // namespace

std::string DirNameComboBox::default_dir_name(const std::string &dir_name) const
{
    return dir_name;
}

bool DirNameComboBox::valid_current_text() const { return valid_path(currentText().trimmed()); }

bool DirNameComboBox::valid_path(const QString &text) const
{
    if (text.endsWith('.') || text.endsWith(' ')) return false;

    // https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
    static const auto reserved_names = {tr("CON"),  tr("PRN"),  tr("AUX"),  tr("NUL"),  tr("COM"),
                                        tr("COM0"), tr("COM1"), tr("COM2"), tr("COM3"), tr("COM4"),
                                        tr("COM5"), tr("COM6"), tr("COM7"), tr("COM8"), tr("COM9"),
                                        tr("LPT0"), tr("LPT1"), tr("LPT2"), tr("LPT3"), tr("LPT4"),
                                        tr("LPT5"), tr("LPT6"), tr("LPT7"), tr("LPT8"), tr("LPT9")};

    for (const auto &name : reserved_names) {
        if (text.startsWith(name, Qt::CaseInsensitive)) {
            if (text.size() == name.size() || text[name.size()] == '.') {
                return false;
            }
        }
    }
    static const auto validator =
        QRegularExpression("^[a-zA-Z0-9_.,&\\-' ]+$", QRegularExpression::CaseInsensitiveOption);

    return validator.match(text).hasMatch();
}

void DirNameComboBox::handle_editing_finished()
{
    auto default_dir = tr(default_dir_name().c_str());
    auto text = currentText();
    QSettings settings("KonteX Neuroscience", "Thor Vision");

    if (!valid_path(text)) {
        spdlog::warn("Invalid directory name entered: '{}'", text.toStdString());

        setItemText(1, default_dir);
        settings.setValue(DIR_NAME, default_dir);
        QTimer::singleShot(0, this, [this, default_dir]() {
            setStyleSheet("");
            setCurrentText(default_dir);
        });

        return;
    }

    auto trimmed_dir_name = text.trimmed();
    setItemText(1, trimmed_dir_name);
    settings.setValue(DIR_NAME, trimmed_dir_name);
}

DirNameComboBox::DirNameComboBox(QWidget *parent, int max_len) : QComboBox(parent)
{
    spdlog::info("Creating DirNameComboBox");

    QSettings settings("KonteX Neuroscience", "Thor Vision");
    auto dir_date = settings.value(DIR_DATE, true).toBool();
    auto dir_name = settings.value(DIR_NAME, tr(default_dir_name().c_str())).toString();
    settings.setValue(DIR_DATE, dir_date);
    settings.setValue(DIR_NAME, dir_name);

    addItem(tr("YYYY-MM-DD_HH-MM-SS"));
    addItem(dir_name);

    setInsertPolicy(QComboBox::NoInsert);
    setEditable(!dir_date);
    setCurrentIndex(dir_date ? 0 : 1);

    if (!dir_date) {
        lineEdit()->setMaxLength(max_len);
        connect(
            lineEdit(), &QLineEdit::editingFinished, this, &DirNameComboBox::handle_editing_finished
        );
        setStyleSheet("");
    } else {
        setStyleSheet("QComboBox { color: gray; }");
    }

    connect(this, &QComboBox::currentIndexChanged, [this, max_len](int index) {
        auto dir_date = (index == 0);
        spdlog::info("Set record directory to {}", dir_date ? "Date" : "Custom");
        QSettings("KonteX Neuroscience", "Thor Vision").setValue(DIR_DATE, dir_date);
        setEditable(!dir_date);

        if (dir_date) {
            setStyleSheet("QComboBox { color: gray; }");
            return;
        }

        lineEdit()->setMaxLength(max_len);
        connect(
            lineEdit(), &QLineEdit::editingFinished, this, &DirNameComboBox::handle_editing_finished
        );
        setStyleSheet("");
    });
    connect(this, &QComboBox::editTextChanged, [this](const QString &text) {
        if (currentIndex() == 0) return;
        lineEdit()->setStyleSheet(valid_path(text) ? "" : "background-color: #FFE4E1;");
    });
}