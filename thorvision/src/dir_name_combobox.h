#pragma once

#include <QComboBox>


class DirNameComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit DirNameComboBox(QWidget *parent = nullptr);
    ~DirNameComboBox() = default;

private:
    bool valid_dir_name_from_user_string(const QString &text);

private slots:
    void handle_editing_finished();
};
