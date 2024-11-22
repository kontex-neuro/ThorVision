#pragma once

#include <string>
#include <vector>

class Camera
{
public:
    enum class Status { Idle, Occupied, Playing, Recording };

    struct Cap {
        std::string media_type;
        std::string format;
        int width;
        int height;
        // std::string width;
        // std::string height;
        int fps_n;
        int fps_d;
    } cap;

    // Camera();
    // Camera(int id);
    Camera(const int _id, const std::string &_name);
    ~Camera() = default;

    static std::string list_cameras(const std::string &url);

    Status get_status() { return status; };
    std::string get_name() { return name; };
    void set_name(std::string _name) { name = _name; };

    std::vector<Cap> &get_caps() { return caps; };
    int get_port() { return port; };
    int get_id() { return id; }
    std::string get_current_cap() { return current_cap; };

    void set_current_cap(const std::string &cap) { current_cap = cap; }
    void add_cap(const Cap &cap) { caps.emplace_back(cap); }

    void change_status(Status _status);

    void start();
    void stop();

private:
    int id;
    int port;
    std::string name;
    std::vector<Cap> caps;
    std::string current_cap;
    Status status;
};