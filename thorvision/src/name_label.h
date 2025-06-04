#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QString>
#include <QWidget>

class NameLabel : public QLabel
{
    Q_OBJECT

public:
    explicit NameLabel(const QString &text, QWidget *parent = nullptr);

private:
    QLineEdit *_editor;

signals:
    void name_changed(const QString &new_name);

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;
};
