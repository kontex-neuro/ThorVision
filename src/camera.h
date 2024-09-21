#pragma once

#include <string>

class Camera
{
public:
    enum class Status { Idle, Occupied, Playing, Recording };

    Camera();
    ~Camera() = default;

    static std::string list_cameras(std::string url);

    Status get_status();
    void change_status(Status _status);
    void start();
    void stop();

    // private:
    int id;
    std::string capability;
    int port;
    Status status;
};