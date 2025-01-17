#include "dir_name_combobox.h"

#include <QDateTime>
#include <QLineEdit>
#include <QSettings>


namespace
{
auto constexpr DIR_NAME = "dir_name";
auto constexpr DIR_DATE = "dir_date";
auto constexpr MAX_DIR_NAME_LEN = 255;
}  // namespace

bool DirNameComboBox::valid_dir_name_from_user_string(const QString &text)
{
    if (text.endsWith('.') || text.endsWith(' ')) {
        return false;
    }
    static const auto reserved_names = {tr("CON"),  tr("PRN"),  tr("AUX"),  tr("NUL"),  tr("COM"),
                                        tr("COM0"), tr("COM1"), tr("COM2"), tr("COM3"), tr("COM4"),
                                        tr("COM5"), tr("COM6"), tr("COM7"), tr("COM8"), tr("COM9"),
                                        tr("LPT0"), tr("LPT1"), tr("LPT2"), tr("LPT3"), tr("LPT4"),
                                        tr("LPT5"), tr("LPT6"), tr("LPT7"), tr("LPT8"), tr("LPT9")};
    for (const auto &reserved : reserved_names) {
        if (text.startsWith(reserved, Qt::CaseInsensitive)) {
            if (text.size() == reserved.size() || text[reserved.size()] == '.') {
                return false;
            }
        }
    }
    static const auto validator =
        QRegularExpression("^[a-zA-Z0-9_\\.,/&\\-' ]+$", QRegularExpression::CaseInsensitiveOption);

    return validator.match(text).hasMatch();
}

void DirNameComboBox::handle_editing_finished()
{
    auto default_dir_name = tr("directory_name");
    auto text = currentText();
    QSettings settings("KonteX Neuroscience", "Thor Vision");
    if (!valid_dir_name_from_user_string(text)) {
        settings.setValue(DIR_NAME, default_dir_name);
        setItemText(1, default_dir_name);
        setCurrentText(default_dir_name);
        return;
    }
    auto dir_name = text.trimmed();
    setItemText(1, dir_name);
    settings.setValue(DIR_NAME, dir_name);
}

DirNameComboBox::DirNameComboBox(QWidget *parent) : QComboBox(parent)
{
    QSettings settings("KonteX Neuroscience", "Thor Vision");
    auto dir_date = settings.value(DIR_DATE, true).toBool();
    auto dir_name =
        settings.value(DIR_NAME, QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"))
            .toString();
    settings.setValue(DIR_DATE, dir_date);
    settings.setValue(DIR_NAME, dir_name);

    addItem(tr("YYYY-MM-DD_HH-MM-SS"));
    addItem(dir_name);

    setInsertPolicy(QComboBox::NoInsert);
    setEditable(!dir_date);
    setCurrentIndex(dir_date ? 0 : 1);

    // TODO: duplicate code inside currentIndexChanged signal slot.
    // index 0 has no lineEdit(), only connect slot when index is 1, which is custom dir name
    // code works fine with duplicate &QLineEdit::editingFinished, but it is irritating.
    if (!dir_date) {
        lineEdit()->setMaxLength(MAX_DIR_NAME_LEN);
        connect(
            lineEdit(), &QLineEdit::editingFinished, this, &DirNameComboBox::handle_editing_finished
        );
    }

    connect(this, &QComboBox::currentIndexChanged, [this](int index) {
        auto dir_date = (index == 0);
        QSettings("KonteX Neuroscience", "Thor Vision").setValue(DIR_DATE, dir_date);
        setEditable(!dir_date);

        if (dir_date) {
            setStyleSheet("");
            return;
        }

        lineEdit()->setMaxLength(MAX_DIR_NAME_LEN);
        connect(
            lineEdit(), &QLineEdit::editingFinished, this, &DirNameComboBox::handle_editing_finished
        );
    });
    connect(this, &QComboBox::editTextChanged, [this](const QString &text) {
        if (currentIndex() == 0) return;
        valid_dir_name_from_user_string(text)
            ? setStyleSheet("")
            : setStyleSheet("QComboBox { background-color: #FFE4E1; }");
    });
}