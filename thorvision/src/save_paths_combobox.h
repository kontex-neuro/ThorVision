#pragma once

#include <QComboBox>
#include <QStandardPaths>
#include <QString>
#include <string>

class SavePathsComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit SavePathsComboBox(QWidget *parent = nullptr, int max_items = 10);
    ~SavePathsComboBox() = default;

    std::string default_save_path(
        const QString &default_path =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
    ) const;
    bool valid_current_text() const;

private:
    bool valid_path(const QString &path) const;
    void reset_path(const QString &path);
};