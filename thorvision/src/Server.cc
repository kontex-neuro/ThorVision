#include "Server.h"

#include <spdlog/spdlog.h>

#include <QPointer>
#include <chrono>

Server::Server(QObject *parent) : QObject(parent), _connected(false), _server(xvc::Server())
{
    _thread = std::jthread([this](std::stop_token st) {
        constexpr auto timeout = std::chrono::milliseconds(1000);
        constexpr auto max_retries = 6;
        auto retry = 0;

        while (!st.stop_requested()) {
            const auto connected = _server.root(timeout);

            if (_connected != connected) {
                spdlog::info("XDAQ status: {}", connected ? "Connected" : "Disconnected");
                _connected = connected;

                QMetaObject::invokeMethod(this, [server = QPointer<Server>(this)]() {
                    if (!server) return;
                    emit server->status_change(server->_connected);
                });
            }

            if (!_connected && retry < max_retries) {
                spdlog::debug("Trying to connect to XDAQ (attempt {}/{})", ++retry, max_retries);
                std::this_thread::sleep_for(timeout);
            } else {
                retry = 0;
            }
        }
    });
}

bool Server::check_api_version()
{
    constexpr auto expected_version = xvc::Version(0, 0, 9);
    auto version = _server.api_version();

    if (!version) {
        return false;
    }
    if (version != expected_version) {
        emit api_version_mismatch(QString::fromStdString(version.value().to_string()));
        return false;
    }
    return true;
}