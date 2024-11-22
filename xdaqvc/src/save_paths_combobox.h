#pragma once

#include <QComboBox>

class SavePathsComboBox : public QComboBox
{
    Q_OBJECT
public:
    SavePathsComboBox(QWidget *parent = nullptr);
    ~SavePathsComboBox() = default;
};
