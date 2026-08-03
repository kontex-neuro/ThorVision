#pragma once

#include <QObject>
#include <atomic>
#include <thread>

#include "xdaqvc/server.h"

class Server : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool xdaq_connected READ xdaq_connected NOTIFY status_change)

public:
    explicit Server(QObject *parent = nullptr);
    ~Server() = default;

    bool xdaq_connected() const noexcept { return _connected.load(); }

signals:
    void status_change(bool connected);

private:
    std::jthread _thread;
    std::atomic<bool> _connected;
    xvc::Server _server;
};