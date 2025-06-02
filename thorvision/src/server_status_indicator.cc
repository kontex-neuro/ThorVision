#include "server_status_indicator.h"

#include <spdlog/spdlog.h>
#include <xdaqvc/server.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <thread>


using namespace std::chrono_literals;


ServerStatusIndicator::ServerStatusIndicator(QWidget *parent)
    : QWidget(parent), _current_status(false), _running(true)
{
    spdlog::info("Creating ServerStatusIndicator");
    auto title_text = new QLabel(tr("Server status:"), this);
    auto status_text = new QLabel(tr("Loading..."), this);
    auto layout = new QHBoxLayout(this);
    status_text->setStyleSheet("color: black;");
    layout->addWidget(title_text);
    layout->addWidget(status_text);

    _thread = std::jthread([this, status_text]() {
        auto const timeout = 500ms;
        int retry_count = 0;
        int const max_retries = 90;

        while (_running) {
            auto server = xvc::Server();
            auto status = server.status(timeout);
            auto on = (status == xvc::Status::ON);

            QMetaObject::invokeMethod(
                status_text,
                [on, status_text]() {
                    if (on) {
                        status_text->setText(tr("Available"));
                        status_text->setStyleSheet("color: green;");
                    } else {
                        status_text->setText(tr("Loading..."));
                        status_text->setStyleSheet("color: black;");
                    }
                },
                Qt::QueuedConnection
            );

            if (_current_status == static_cast<bool>(xvc::Status::ON) && status == xvc::Status::OFF && retry_count < max_retries) {
                if(retry_count > 0) {
                    spdlog::info("Server status: OFF => (connecting retry {})", retry_count);
                }
                retry_count++;
                continue;
            }
            else {
                retry_count = 0;
            }

            if (_current_status != on) {
                spdlog::info("Server status: {}", on ? "Available" : "Loading...");
                _current_status = on;
                emit status_change(on);
            }
            std::this_thread::sleep_for(500ms);
        }
    });
}

ServerStatusIndicator::~ServerStatusIndicator() { _running = false; }