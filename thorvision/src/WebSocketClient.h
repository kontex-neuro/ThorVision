#pragma once

#include <QObject>
#include <string>

#include "xdaqvc/ws_client.h"

class WebSocketClient : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient() = default;

signals:
    void camera_added(const std::string &json_text);
    void camera_removed(int camera_id);

private:
    xvc::ws_client _ws_client;
};