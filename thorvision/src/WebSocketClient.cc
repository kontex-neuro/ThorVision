#include "WebSocketClient.h"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

WebSocketClient::WebSocketClient(QObject *parent)
    : QObject(parent), _ws_client("192.168.177.100", "8000", [&](std::string_view event) {
          try {
              const auto &device_event = nlohmann::json::parse(event);
              const auto &event_type = device_event.at("event_type").get<std::string>();
              const auto &camera_json = device_event.at("camera");

              if (event_type == "Added") {
                  QMetaObject::invokeMethod(this, [this, camera_json]() {
                      emit camera_added(camera_json.dump());
                  });
                  //   emit camera_added(camera_json.dump());
              } else if (event_type == "Removed") {
                  const auto id = camera_json.at("id").get<int>();
                  QMetaObject::invokeMethod(this, [this, id]() { emit camera_removed(id); });
                  //   emit camera_removed(id);
              }
          } catch (const nlohmann::json::parse_error &e) {
              spdlog::error("Failed to parse WebSocket message: {}, error: {}", event, e.what());
          }
      })
{
}