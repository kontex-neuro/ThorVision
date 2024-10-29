#include "save_paths_combobox.h"

#include <QDir>
#include <QLineEdit>
#include <QListView>
#include <QSettings>

namespace
{
constexpr auto SAVE_PATHS = "save_paths";
constexpr auto MAX_ITEMS = 10;
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

    QSettings settings("KonteX", "VC");
    auto save_paths = settings.value(SAVE_PATHS, QStringList(QDir::currentPath())).toStringList();
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
        QSettings("KonteX", "VC").setValue(SAVE_PATHS, paths);
    });
}