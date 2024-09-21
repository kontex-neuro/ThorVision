#include "camera.h"

#include <cpr/api.h>
#include <fmt/core.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>


using nlohmann::json;
using namespace std::chrono_literals;

Camera::Camera() : id(0), port(0), status(Status::Idle) {}

std::string Camera::list_cameras(std::string url)
{
    fmt::println("Camera::list_cameras");

    auto response = cpr::Get(cpr::Url{url}, cpr::Timeout{1s});
    // assert(response.elapsed <= 1);
    if (response.status_code == 200) {
        return json::parse(response.text).dump(2);
    }
    return "";
}

Camera::Status Camera::get_status() { return status; }

void Camera::change_status(Status _status)
{
    fmt::println("Camera::change_status");
    status = _status;
    
    json json_body;
    json_body["id"] = id;
    json_body["status"] = status;
    auto response = cpr::Put(
        cpr::Url{"192.168.177.100:8000/camera"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{json_body.dump(2)},
        cpr::Timeout{1s}
    );
    if (response.status_code == 200) {
        fmt::println("successfully change camera status");
    }
}
void Camera::start()
{
    fmt::println("Camera::start");

    json json_body;
    json_body["id"] = id;
    json_body["capability"] = capability;
    json_body["port"] = port;
    auto response = cpr::Post(
        cpr::Url{"192.168.177.100:8000/start"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{json_body.dump(2)},
        cpr::Timeout{1s}
    );
    if (response.status_code == 200) {
        fmt::println("successfully start stream");
    }
}

void Camera::stop()
{
    fmt::println("Camera::stop");

    json json_body;
    json_body["id"] = id;
    auto response = cpr::Post(
        cpr::Url{"192.168.177.100:8000/stop"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{json_body.dump(2)},
        cpr::Timeout{1s}
    );
    if (response.status_code == 200) {
        fmt::println("successfully stop stream");
    }
}