#include "Server.h"

#include <spdlog/spdlog.h>

#include <QPointer>
#include <chrono>

Server::Server(QObject *parent) : QObject(parent), _connected(false)
{
    _thread = std::jthread([this](std::stop_token st) {
        constexpr auto timeout = std::chrono::milliseconds(500);
        constexpr auto max_retries = 6;
        auto retry = 0;

        while (!st.stop_requested()) {
            const auto connected = _server.root(timeout);

            if (!connected && retry < max_retries) {
                spdlog::debug("Trying to connect to XDAQ (attempt {}/{})", ++retry, max_retries);
                std::this_thread::sleep_for(timeout);
                continue;
            }
            retry = 0;

            if (_connected.load() != connected) {
                spdlog::info("XDAQ status: {}", connected ? "Connected" : "Disconnected");
                _connected.store(connected);

                QMetaObject::invokeMethod(this, [server = QPointer<Server>(this), connected]() {
                    if (!server) return;
                    emit server->status_change(connected);
                });
            }

            std::this_thread::sleep_for(timeout * 6);
        }
    });
}
