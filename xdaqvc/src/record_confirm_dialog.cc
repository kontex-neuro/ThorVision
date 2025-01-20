#include "record_confirm_dialog.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>


RecordConfirmDialog::RecordConfirmDialog(const QString &specs, QWidget *parent)
    : QDialog(parent), _dont_ask_again(false)
{
    setWindowTitle("Record Settings Confirm");

    auto layout = new QVBoxLayout(this);

    auto info = new QLabel(
        "Are you sure you want to start recording with the following camera settings?\n\n" + specs,
        this
    );

    auto button_layout = new QHBoxLayout();
    auto dont_ask_again_checkbox = new QCheckBox(tr("Don't ask me again"), this);
    auto ok = new QPushButton(tr("OK"), this);
    auto cancel = new QPushButton(tr("Cancel"), this);

    info->setWordWrap(true);
    ok->setFixedWidth(ok->sizeHint().width());
    cancel->setFixedWidth(cancel->sizeHint().width());
    button_layout->addWidget(dont_ask_again_checkbox);
    button_layout->addWidget(ok);
    button_layout->addWidget(cancel);
    layout->addWidget(info);
    layout->addLayout(button_layout);

    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(dont_ask_again_checkbox, &QCheckBox::toggled, [this](bool checked) {
        _dont_ask_again = checked;
    });
}