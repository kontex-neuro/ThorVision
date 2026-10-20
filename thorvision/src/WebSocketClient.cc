#include "WebSocketClient.h"
#include "xdaqvc/recording.h"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

WebSocketClient::WebSocketClient(QObject *parent) : QObject(parent) { connect_client(); }

void WebSocketClient::connect_client()
{
    _ws_client = std::make_unique<xvc::ws_client>(
        xvc::device_host(), std::to_string(xvc::device_port()),
        [this](std::string_view event) {
            try {
                const auto &device_event = nlohmann::json::parse(event);
                const auto &event_type = device_event.at("event_type").get<std::string>();
                if (event_type == "RecordingState") return; // DeviceRecording reconciles full snapshots.
                const auto &camera_json = device_event.at("camera");

                if (event_type == "Added") {
                    QMetaObject::invokeMethod(this, [this, camera_json]() {
                        emit camera_added(camera_json.dump());
                    });
                } else if (event_type == "Removed") {
                    const auto id = camera_json.at("id").get<int>();
                    QMetaObject::invokeMethod(this, [this, id]() { emit camera_removed(id); });
                }
            } catch (const nlohmann::json::exception &e) {
                spdlog::error(
                    "Failed to parse WebSocket message: {}, error: {}", event, e.what()
                );
            }
        }
    );
}

void WebSocketClient::reconnect()
{
    spdlog::info("Reconnecting camera event WebSocket");
    // Destroying the old client joins its io_context thread before the new one dials, so the
    // two never overlap.
    _ws_client.reset();
    connect_client();
}
