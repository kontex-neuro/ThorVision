#pragma once

#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <atomic>
#include <thread>

class Server : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool xdaq_connected READ xdaq_connected NOTIFY status_change)

public:
    explicit Server(QObject *parent = nullptr);
    ~Server();

    bool xdaq_connected() const { return _current_status; }

private:
    std::jthread _thread;
    bool _current_status;
    std::atomic_bool _running;

signals:
    void status_change(bool connected);
};

#endif