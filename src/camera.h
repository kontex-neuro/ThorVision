#pragma once

#include <string>
#include <vector>

class Camera
{
public:
    enum class Status { Idle, Occupied, Playing, Recording };

    // Camera();
    // Camera(int id);
    Camera(const int _id, const std::string &_name);
    ~Camera() = default;

    static std::string list_cameras(const std::string &url);

    Status get_status() { return status; };
    std::string get_name() { return name; };
    std::vector<std::string> &get_capabilities() { return capabilities; };
    int get_port() { return port; };
    void set_current_cap(std::string cap) { current_cap = cap; }

    void change_status(Status _status);
    void add_capability(const std::string &cap);

    void start();
    void stop();

private:
    int id;
    std::string name;
    std::vector<std::string> capabilities;
    std::string current_cap;
    int port;
    Status status;
};