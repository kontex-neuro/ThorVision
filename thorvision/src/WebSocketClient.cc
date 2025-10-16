#include "WebSocketClient.h"

WebSocketClient::WebSocketClient(QObject *parent) : QObject(parent)
{
    _ws_client =
        std::make_unique<xvc::ws_client>("192.168.177.100", "8000", [&](std::string_view event) {
            auto const device_event = json::parse(event);
            auto const event_type = device_event["event_type"];
            auto const camera_json = device_event["camera"];

            if (event_type == "Added") {
                emit camera_added(camera_json);
            } else if (event_type == "Removed") {
                auto const id = camera_json["id"].get<int>();
                emit camera_removed(id);
            }
        });
}