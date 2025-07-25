#include <spdlog/spdlog.h>

#include <QtGui>
#include <QtQml>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "ImageProvider.h"
#include "Recorder.h"
#include "Server.h"
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
    auto recorder = new Recorder(&app);
    auto server = new Server(&app);

    engine.rootContext()->setContextProperty("CameraModel", camera_model);
    engine.rootContext()->setContextProperty("Recorder", recorder);
    engine.rootContext()->setContextProperty("Server", server);

    QObject::connect(
        server,
        &Server::status_change,
        &app,
        [camera_model, provider](bool connected) {
            if (connected) {
                for (auto *cam : Camera::cameras()) {
                    camera_model->add_camera(cam, provider);
                }
            } else {
                for (int i = camera_model->count() - 1; i >= 0; --i) {
                    camera_model->remove_camera(i);
                }
            }
        }
    );

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
        std::make_unique<xvc::ws_client>([camera_model, provider, recorder](const std::string &event
                                         ) {
            auto const device_event = json::parse(event);
            auto const event_type = device_event["event_type"];
            auto const camera_json = device_event["camera"];

            if (event_type == "Added") {
                auto camera = Camera::parse(camera_json);
                camera_model->add_camera(camera, provider);
            } else if (event_type == "Removed") {
                auto const id = camera_json["id"].get<int>();
                auto const index = camera_model->index_of_camera_id(id);

                if (index == -1) {
                    spdlog::error("Camera: id {} not found", id);
                    return;
                }

                auto camera_name = camera_model->get(index)["name"].toString();
                camera_model->remove_camera(index);
                if (recorder->recording()) {
                    emit camera_model->camera_unplugged_during_recording(camera_name);
                }
            }
        });

    return app.exec();
}