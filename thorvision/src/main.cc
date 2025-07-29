#include <spdlog/spdlog.h>

#include <QtGui>
#include <QtQml>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "ImageProvider.h"
#include "Recorder.h"
#include "Server.h"
#include "WebSocketClient.h"

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    qmlRegisterType<CameraItem>("App", 1, 0, "CameraItem");

    auto provider = new ImageProvider();

    engine.addImageProvider("video", provider);

    auto camera_model = new CameraModel(&app);
    auto recorder = new Recorder(&app);
    auto server = new Server(&app);

    // TODO: when closing the app, the following error occurs:
    // libc++abi: terminating due to uncaught exception of type std::__1::system_error: mutex lock
    // failed: Invalid argument
    auto ws_client = new WebSocketClient(&app);

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

    QObject::connect(
        ws_client,
        &WebSocketClient::camera_added,
        camera_model,
        [camera_model, provider](const json &camera_json) {
            auto const camera = Camera::parse(camera_json);
            camera_model->add_camera(camera, provider);
        }
    );

    QObject::connect(
        ws_client,
        &WebSocketClient::camera_removed,
        camera_model,
        [camera_model, recorder](int id) {
            auto const index = camera_model->index_of_camera_id(id);
            if (index == -1) {
                spdlog::error("Camera: id {} not found", id);
                return;
            }

            auto const camera_name = camera_model->get(index)["name"].toString();
            camera_model->remove_camera(index);
            if (recorder->recording()) {
                emit camera_model->camera_unplugged_during_recording(camera_name);
            }
        }
    );

    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });

    return app.exec();
}