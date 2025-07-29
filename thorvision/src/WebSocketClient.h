#pragma once

#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QObject>
#include <memory>
#include <nlohmann/json.hpp>

#include "xdaqvc/ws_client.h"

using json = nlohmann::json;

class WebSocketClient : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient() = default;

signals:
    void camera_added(const json &camera_json);
    void camera_removed(int camera_id);

private:
    std::unique_ptr<xvc::ws_client> _ws_client;
};

#endif