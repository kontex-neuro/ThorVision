#pragma once

#include <QComboBox>
#include <QString>
#include <QWidget>
#include <string>

class DirNameComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit DirNameComboBox(QWidget *parent = nullptr, int max_len = 255);
    ~DirNameComboBox() = default;

    std::string default_dir_name(const std::string &dir_name = "directory_name") const;
    bool valid_current_text() const;

private:
    bool valid_path(const QString &text) const;

private slots:
    void handle_editing_finished();
};
