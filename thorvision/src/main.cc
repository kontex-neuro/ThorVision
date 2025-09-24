#include <spdlog/spdlog.h>

#include <QtGui>
#include <QtQml>
#include <csignal>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "ImageProvider.h"
#include "Recorder.h"
#include "RecorderSettings.h"
#include "Server.h"
#include "WebSocketClient.h"

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

#ifdef __APPLE__
    // set GST_PLUGIN_PATH to find GStreamer plugins inside the bundle
    setenv("GST_PLUGIN_PATH", (app.applicationDirPath() + "/../PlugIns/gstreamer").toUtf8(), true);
#endif

    if (!gst_is_initialized()) {
        gst_init(&argc, &argv);
    }

    auto provider = new ImageProvider();

    engine.addImageProvider("video", provider);

    auto camera_model = new CameraModel(&engine);
    auto recorder_settings = new RecorderSettings(&engine);
    auto recorder = new Recorder(camera_model, recorder_settings, &engine);
    auto server = new Server(&engine);
    // TODO: when closing the app, the following error occurs:
    // libc++abi: terminating due to uncaught exception of type std::__1::system_error: mutex lock
    // failed: Invalid argument
    auto ws_client = new WebSocketClient(&engine);

    engine.rootContext()->setContextProperty("CameraModel", camera_model);
    engine.rootContext()->setContextProperty("Recorder", recorder);
    engine.rootContext()->setContextProperty("RecorderSettings", recorder_settings);
    engine.rootContext()->setContextProperty("Server", server);
    // engine.rootContext()->setContextProperty("WebSocketClient", ws_client);

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
                for (auto i = camera_model->rowCount() - 1; i >= 0; --i) {
                    camera_model->remove_camera(i);
                }
            }
        }
    );

    const QUrl url(QStringLiteral("qrc:/qt/qml/App/Theme/src/main.qml"));
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

            auto const model_index = camera_model->index(index);
            auto const camera_name =
                camera_model->data(model_index, CameraModel::NameRole).toString();
            camera_model->remove_camera(index);
            if (recorder->recording()) {
                emit camera_model->camera_unplugged_during_recording(camera_name);
            }
        }
    );

    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });

    return app.exec();
}