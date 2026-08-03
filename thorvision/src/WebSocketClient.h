#pragma once

#include <QObject>
#include <memory>
#include <string>

#include "xdaqvc/ws_client.h"

class WebSocketClient : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient() = default;

    // Tears down the connection and dials again.
    //
    // A device update restarts the server this socket is attached to. The old connection
    // dies with it and never comes back on its own, so every camera hotplug event after an
    // update is lost until the application is restarted. Recreating the client is the only
    // reconnect handle libxvc exposes.
    void reconnect();

signals:
    void camera_added(const std::string &json_text);
    void camera_removed(int camera_id);

private:
    void connect_client();

    std::unique_ptr<xvc::ws_client> _ws_client;
};