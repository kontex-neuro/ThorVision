#pragma once

#include <QObject>
#include <thread>

#include "xdaqvc/server.h"

class Server : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool xdaq_connected READ xdaq_connected NOTIFY status_change)

public:
    explicit Server(QObject *parent = nullptr);
    ~Server() {};

    bool check_api_version();
    bool xdaq_connected() const noexcept { return _connected; }

signals:
    void status_change(bool connected);
    void api_version_mismatch(const QString &version);

private:
    std::jthread _thread;
    bool _connected;
    xvc::Server _server;
};