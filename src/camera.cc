#include "camera.h"

#include <cpr/api.h>
#include <fmt/core.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "../libxvc.h"
#include "spdlog/spdlog.h"


using nlohmann::json;
using namespace std::chrono_literals;

Camera::Camera(const int _id, const std::string &_name)
    : id(_id), name(_name), port(xvc::port_pool->allocate_port()), status(Status::Idle)
{
}

std::string Camera::list_cameras(const std::string &url)
{
    spdlog::info("Camera::list_cameras");

    auto response = cpr::Get(cpr::Url{url}, cpr::Timeout{3s});
    // assert(response.elapsed <= 1);
    if (response.status_code == 200) {
        return json::parse(response.text).dump(2);
    }
    return "";
}

void Camera::change_status(Status _status)
{
    spdlog::info("Camera::change_status");
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
    spdlog::info("Camera::start");

    json json_body;
    json_body["id"] = id;
    json_body["capability"] = current_cap;
    json_body["port"] = port;
    cpr::Url url;
    if (current_cap.find("image/jpeg") != std::string::npos) {
        url = cpr::Url{"192.168.177.100:8000/jpeg"};
    } else if (id == -1) {
        url = cpr::Url{"192.168.177.100:8000/mock"};
    } else {
        url = cpr::Url{"192.168.177.100:8000/h265"};
    }
    auto response = cpr::Post(
        url,
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
    spdlog::info("Camera::stop");

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