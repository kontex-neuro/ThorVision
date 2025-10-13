#include "Server.h"

#include "spdlog/spdlog.h"
#include "xdaqvc/server.h"

Server::Server(QObject *parent) : QObject(parent), _current_status(false), _running(true)
{
    auto server = xvc::Server();

    _thread = std::jthread([this, server]() {
        auto const timeout = 500ms;
        auto retry = 0;
        auto const max_retries = 6;

        while (_running) {
            auto status = server.status(timeout);
            auto on = (status == xvc::Status::ON);

            if (_current_status && status == xvc::Status::OFF && retry < max_retries) {
                spdlog::info("Connecting retry: {}", ++retry);

                std::this_thread::sleep_for(timeout);
                continue;
            } else {
                retry = 0;
            }

            if (_current_status != on) {
                spdlog::info("XDAQ status: {}", on ? "Connected" : "Connecting");
                _current_status = on;

                QMetaObject::invokeMethod(this, [this, on]() { emit status_change(on); });
            }

            std::this_thread::sleep_for(timeout);
        }
    });
}

Server::~Server() { _running = false; }