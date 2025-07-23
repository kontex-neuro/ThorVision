#include <spdlog/spdlog.h>

#include <QtGui>
#include <QtQml>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "ImageProvider.h"
#include "xdaqvc/camera.h"
#include "xdaqvc/ws_client.h"

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    auto provider = new ImageProvider();

    engine.addImageProvider("video", provider);

    auto camera_model = new CameraModel(&app);

    for (auto &cam : Camera::cameras()) {
        camera_model->add_camera(cam, provider);
    }

    engine.rootContext()->setContextProperty("CameraModel", camera_model);

    const QUrl url(QStringLiteral("thorvision/src/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    engine.load(url);
    if (engine.rootObjects().isEmpty()) return -1;

    auto ws_client =
        std::make_unique<xvc::ws_client>([camera_model, provider](const std::string &event) {
            auto const device_event = json::parse(event);
            auto const event_type = device_event["event_type"];
            auto const camera_json = device_event["camera"];

            if (event_type == "Added") {
                auto camera = Camera::parse(camera_json);
                camera_model->add_camera(camera, provider);
            } else if (event_type == "Removed") {
                auto const id = camera_json["id"].get<int>();
                auto index = camera_model->index_of_camera_id(id);
                if (index != -1) {
                    camera_model->remove_camera(index);
                } else {
                    spdlog::error("Camera with id {} not found", id);
                }
            }
        });

    return app.exec();
}