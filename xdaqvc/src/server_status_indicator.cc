#include "server_status_indicator.h"

#include <spdlog/spdlog.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <thread>


using namespace std::chrono_literals;


ServerStatusIndicator::ServerStatusIndicator(QWidget *parent)
    : QWidget(parent), _current_status(false), _running(true)
{
    auto title_text = new QLabel(tr("Server status:"), this);
    auto status_text = new QLabel(tr("OFF"), this);
    auto layout = new QHBoxLayout(this);
    layout->addWidget(title_text);
    layout->addWidget(status_text);

    _thread = std::jthread([this, status_text]() {
        auto timeout = 500ms;

        while (_running) {
            auto status = xvc::server_status(timeout);
            auto on = (status == xvc::Status::ON);

            if (_current_status != on) {
                spdlog::debug("Server status: {}", on ? "ON" : "OFF");
                _current_status = on;

                QMetaObject::invokeMethod(
                    status_text,
                    [on, status_text]() { status_text->setText(on ? tr("ON") : tr("OFF")); },
                    Qt::QueuedConnection
                );
                emit status_change(on);
            }
            std::this_thread::sleep_for(500ms);
        }
    });
}

ServerStatusIndicator::~ServerStatusIndicator() { _running = false; }