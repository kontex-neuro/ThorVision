#pragma once

#include <xdaqvc/server.h>

#include <QWidget>
#include <atomic>
#include <thread>


class ServerStatusIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit ServerStatusIndicator(QWidget *parent = nullptr);
    ~ServerStatusIndicator();

private:
    std::jthread _thread;
    bool _current_status;
    std::atomic_bool _running;

signals:
    void status_change(bool is_server_on);
};