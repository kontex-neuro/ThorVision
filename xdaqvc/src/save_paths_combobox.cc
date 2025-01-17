#include "save_paths_combobox.h"

#include <QFileInfo>
#include <QLineEdit>
#include <QListView>
#include <QSettings>
#include <QStandardPaths>

namespace
{
auto constexpr SAVE_PATHS = "save_paths";
auto constexpr MAX_ITEMS = 10;
}  // namespace

bool valid_save_path_from_user_string(const QString &text)
{
    if (text.isEmpty()) return false;

    QFileInfo fileInfo(text);
    if (fileInfo.exists()) {
        return fileInfo.isDir();
    }
    return QFileInfo(fileInfo.path()).exists();
}

SavePathsComboBox::SavePathsComboBox(QWidget *parent) : QComboBox(parent)
{
    setEditable(true);
    setMaxVisibleItems(MAX_ITEMS);
    setMaxCount(MAX_ITEMS);
    setFixedWidth(420);

    auto view = new QListView();
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setView(view);

    QSettings settings("KonteX Neuroscience", "Thor Vision");
    auto save_paths =
        settings
            .value(
                SAVE_PATHS,
                QStringList(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            )
            .toStringList();
    addItems(save_paths);
    settings.setValue(SAVE_PATHS, save_paths);

    connect(this, &QComboBox::editTextChanged, [this](const QString &text) {
        valid_save_path_from_user_string(text)
            ? setStyleSheet("")
            : setStyleSheet("QComboBox { background-color: #FFE4E1; }");
    });
    connect(lineEdit(), &QLineEdit::editingFinished, [this]() {
        auto path = currentText().trimmed();
        if (!valid_save_path_from_user_string(path)) return;

        auto path_index = findText(path);
        if (path_index != -1) {
            removeItem(path_index);
        }
        insertItem(0, path);
        setCurrentIndex(0);

        auto paths = QStringList();
        for (int i = 0; i < count(); ++i) {
            paths << itemText(i);
        }
        QSettings("KonteX Neuroscience", "Thor Vision").setValue(SAVE_PATHS, paths);
    });
}