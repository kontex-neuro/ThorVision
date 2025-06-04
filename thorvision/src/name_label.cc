#include "name_label.h"

#include <spdlog/spdlog.h>

#include <QCursor>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

NameLabel::NameLabel(const QString &text, QWidget *parent) : QLabel(text, parent), _editor(nullptr)
{
    spdlog::info("Creating NameLabel");

    QCursor cursor(Qt::IBeamCursor);
    setCursor(cursor);
}

void NameLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        auto current_name = text();
        spdlog::info("NameLabel double clicked: {}", current_name.toStdString());

        const auto regex = QRegularExpression(
            "^[a-zA-Z0-9_.,&\\-' ]*$", QRegularExpression::CaseInsensitiveOption
        );

        _editor = new QLineEdit(this);
        _editor->setMaxLength(20);
        _editor->setText(current_name);

        auto validator = new QRegularExpressionValidator(regex, _editor);
        _editor->setValidator(validator);

        _editor->setGeometry(rect());
        _editor->setFocus();
        _editor->selectAll();
        _editor->show();

        connect(_editor, &QLineEdit::editingFinished, this, [this, current_name]() {
            auto new_name = _editor->text();
            if (new_name.isEmpty()) {
                setText(current_name);
                _editor->deleteLater();
                _editor = nullptr;
                return;
            }

            spdlog::info("NameLabel text changed to: {}", new_name.toStdString());
            setText(new_name);

            emit name_changed(new_name);

            _editor->deleteLater();
            _editor = nullptr;
        });

        e->accept();
    }
}