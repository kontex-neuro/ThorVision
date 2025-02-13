#pragma once

#include <QDialog>


class RecordConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    RecordConfirmDialog(const QString &specs, QWidget *parent = nullptr);

    bool dont_ask_again() const { return _dont_ask_again; }

private:
    bool _dont_ask_again;
};