#include "camera.h"

#include <cpr/api.h>

#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "spdlog/spdlog.h"



using nlohmann::json;
using namespace std::chrono_literals;

Camera::Camera(const int _id, const std::string &_name)
    : id(_id), port(xvc::port_pool->allocate_port()), name(_name)
{
}

std::string Camera::list_cameras(const std::string &url)
{
    auto response = cpr::Get(cpr::Url{url}, cpr::Timeout{2s});
    if (response.status_code == 200) {
        return json::parse(response.text).dump(2);
    }
    return std::string("");
}

void Camera::start()
{
    json payload;
    payload["id"] = id;
    payload["capability"] = current_cap;
    payload["port"] = port;
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
        cpr::Body{payload.dump(2)},
        cpr::Timeout{2s}
    );
    if (response.status_code == 200) {
        spdlog::info("Successfully start camera");
    }
}

void Camera::stop()
{
    json payload;
    payload["id"] = id;
    auto response = cpr::Post(
        cpr::Url{"192.168.177.100:8000/stop"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{payload.dump(2)},
        cpr::Timeout{2s}
    );
    if (response.status_code == 200) {
        spdlog::info("Successfully stop camera");
    }
}