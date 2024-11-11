#pragma once

#include <string>
#include <vector>

class Camera
{
public:
    struct Cap {
        std::string media_type;
        std::string format;
        int width;
        int height;
        int fps_n;
        int fps_d;
    };

    Camera(const int _id, const std::string &_name);
    ~Camera() = default;

    static std::string list_cameras(const std::string &url);

    const std::string &get_name() const { return name; };
    const std::vector<Cap> &get_caps() const { return caps; };
    int get_port() const { return port; };
    int get_id() const { return id; }
    const std::string &get_current_cap() const { return current_cap; };

    void set_current_cap(const std::string &cap) { current_cap = cap; }
    void add_cap(const Cap &cap) { caps.emplace_back(cap); }

    void start();
    void stop();

private:
    int id;
    int port;
    std::string name;
    std::vector<Cap> caps;
    std::string current_cap;
};