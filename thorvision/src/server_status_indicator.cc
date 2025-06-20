#include "server_status_indicator.h"

#include <spdlog/spdlog.h>

#include <QHBoxLayout>
#include <QLabel>

#include "xdaqvc/server.h"

using namespace std::chrono_literals;

ServerStatusIndicator::ServerStatusIndicator(QWidget *parent)
    : QWidget(parent), _current_status(false), _running(true)
{
    spdlog::info("Creating ServerStatusIndicator");

    auto server = xvc::Server();
    auto title_text = new QLabel(tr("XDAQ status:"), this);
    auto status_text = new QLabel(tr("Connecting."), this);
    auto layout = new QHBoxLayout(this);

    status_text->setMinimumWidth(75);
    status_text->setStyleSheet("color: black;");

    layout->addWidget(title_text);
    layout->addWidget(status_text);

    _thread = std::jthread([this, status_text, server]() {
        const QStringList loading_states = {
            tr("Connecting."), tr("Connecting.."), tr("Connecting...")
        };
        auto const timeout = 500ms;
        auto retry = 0;
        auto const max_retries = 6;
        auto loading_step = 0;

        while (_running) {
            auto status = server.status(timeout);
            auto on = (status == xvc::Status::ON);

            QMetaObject::invokeMethod(
                status_text,
                [on, status_text, &loading_step, &loading_states]() {
                    if (on) {
                        status_text->setText(tr("Connected"));
                        status_text->setStyleSheet("color: green;");
                    } else {
                        status_text->setText(loading_states[loading_step]);
                        status_text->setStyleSheet("color: black;");
                    }
                },
                Qt::QueuedConnection
            );

            if (!on) {
                loading_step = (loading_step + 1) % loading_states.size();
            }

            if (_current_status == static_cast<bool>(xvc::Status::ON) &&
                status == xvc::Status::OFF && retry < max_retries) {
                ++retry;
                spdlog::info("Connecting retry: {}", retry);

                std::this_thread::sleep_for(timeout);
                continue;
            } else {
                retry = 0;
            }

            if (_current_status != on) {
                spdlog::info("XDAQ status: {}", on ? "Connected" : "Connecting.");
                _current_status = on;
                emit status_change(on);
            }

            std::this_thread::sleep_for(timeout);
        }
    });
}

ServerStatusIndicator::~ServerStatusIndicator()
{
    spdlog::info("Destroying ServerStatusIndicator");
    _running = false;
}