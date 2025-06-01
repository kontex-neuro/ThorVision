#include "save_paths_combobox.h"

#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QFileInfo>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QSettings>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
auto constexpr SAVE_PATHS = "save_paths";
}  // namespace

std::string SavePathsComboBox::default_save_path(const QString &default_path) const
{
    auto path = fs::path(default_path.toStdString()) / "Thor Vision";
    return path.generic_string();
}

bool SavePathsComboBox::valid_current_text() const { return valid_path(currentText().trimmed()); }

bool SavePathsComboBox::valid_path(const QString &text) const
{
    QFileInfo file_info(text);
    return !text.isEmpty() && file_info.exists() && file_info.isDir() && file_info.isWritable();
}

SavePathsComboBox::SavePathsComboBox(QWidget *parent, int max_items) : QComboBox(parent)
{
    spdlog::info("Creating SavePathsComboBox");

    setEditable(true);
    setMaxVisibleItems(max_items);
    setMaxCount(max_items);
    setFixedWidth(420);

    auto view = new QListView(this);
    setView(view);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto default_path = default_save_path();
    if (!fs::exists(default_path)) {
        std::error_code ec;
        spdlog::info("Create Directory: {}", default_path);
        if (!fs::create_directory(default_path, ec)) {
            spdlog::info("Failed to create directory: {}. Error: {}", default_path, ec.message());
        }
    }

    QSettings settings("KonteX Neuroscience", "Thor Vision");
    auto save_paths = settings.value(SAVE_PATHS, QStringList(QString::fromStdString(default_path)))
                          .toStringList();
    addItems(save_paths);
    settings.setValue(SAVE_PATHS, save_paths);

    connect(this, &QComboBox::editTextChanged, [this](const QString &text) {
        lineEdit()->setStyleSheet(valid_path(text) ? "" : "background-color: #FFE4E1;");
    });
    connect(
        lineEdit(),
        &QLineEdit::editingFinished,
        [this, default_path = QString::fromStdString(default_save_path())]() {
            auto path = currentText().trimmed();
            spdlog::info("LineEdit 'SavePathsComboBox' selected path: {}", path.toStdString());

            if (!valid_path(path)) {
                spdlog::warn("Invalid path entered: '{}'", path.toStdString());

                // QMessageBox::warning(
                //     this,
                //     tr("Invalid Save Path"),
                //     tr("The save path you entered is not valid.\n"
                //        "It has been reset to the default location:\n%1")
                //         .arg(default_path)
                // );

                if (findText(default_path) == -1) {
                    insertItem(0, default_path);
                }
                setCurrentText(default_path);
                setCurrentIndex(findText(default_path));
                lineEdit()->setStyleSheet("");

                QStringList paths;
                for (auto i = 0; i < count(); ++i) {
                    paths << itemText(i);
                }
                if (!paths.contains(default_path)) paths.prepend(default_path);
                QSettings("KonteX Neuroscience", "Thor Vision").setValue(SAVE_PATHS, paths);

                return;
            }

            auto path_index = findText(path);
            if (path_index != -1) {
                removeItem(path_index);
            }
            insertItem(0, path);
            setCurrentIndex(0);

            auto paths = QStringList();
            for (auto i = 0; i < count(); ++i) {
                paths << itemText(i);
            }
            QSettings("KonteX Neuroscience", "Thor Vision").setValue(SAVE_PATHS, paths);
        }
    );
}