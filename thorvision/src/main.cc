#include <spdlog/spdlog.h>

#include <QQuickWindow>
#include <QtGui>
#include <QtQml>
#include <csignal>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "Recorder.h"
#include "RecorderSettings.h"
#include "Server.h"
#include "WebSocketClient.h"

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QGuiApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // register Qt6GLVideoItem as qml element
    if (auto sink = gst_element_factory_make("qml6glsink", nullptr)) {
        gst_object_unref(sink);
    }

    QQmlApplicationEngine engine;

#ifdef __APPLE__
#ifdef APP_BUNDLE_INSTALL
    // set GST_PLUGIN_PATH to find GStreamer plugins inside the bundle
    setenv("GST_PLUGIN_PATH", (app.applicationDirPath() + "/../PlugIns/gstreamer").toUtf8(), true);
#endif
#endif

    auto camera_model = new CameraModel(&engine);
    auto recorder_settings = new RecorderSettings(&engine);
    auto recorder = new Recorder(camera_model, recorder_settings, &engine);
    auto server = new Server(&engine);
    // TODO: when closing the app, the following error occurs:
    // libc++abi: terminating due to uncaught exception of type std::__1::system_error: mutex lock
    // failed: Invalid argument
    auto ws_client = new WebSocketClient(&engine);

    auto root_context = engine.rootContext();
    root_context->setContextProperty("CameraModel", camera_model);
    root_context->setContextProperty("Recorder", recorder);
    root_context->setContextProperty("RecorderSettings", recorder_settings);
    root_context->setContextProperty("Server", server);

    const QUrl url(QStringLiteral("qrc:/qt/qml/App/Theme/src/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.load(url);
    if (engine.rootObjects().isEmpty()) return -1;

    QObject::connect(server, &Server::status_change, [camera_model](bool connected) {
        if (connected) {
            for (auto *cam : Camera::cameras()) {
                camera_model->add_camera(cam);
            }
        } else {
            for (auto i = camera_model->rowCount() - 1; i >= 0; --i) {
                camera_model->remove_camera(i);
            }
        }
    });
    QObject::connect(
        ws_client,
        &WebSocketClient::camera_added,
        camera_model,
        [camera_model](const json &camera_json) {
            auto const camera = Camera::parse(camera_json);
            camera_model->add_camera(camera);
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

    auto root_object = static_cast<QQuickWindow *>(engine.rootObjects().first());
    qDebug() << "Found root object:" << root_object;
    g_assert(root_object);

    auto video_layout = root_object->findChild<QQuickItem *>("video_layout");
    qDebug() << "Found video layout:" << video_layout;
    g_assert(video_layout);

    auto repeater = video_layout->findChild<QQuickItem *>("repeater");
    qDebug() << "Found repeater:" << repeater;
    g_assert(repeater);

    QObject::connect(
        repeater,
        SIGNAL(itemAdded(int, QQuickItem *)),
        camera_model,
        SLOT(onItemAdded(int, QQuickItem *))
    );
    // QObject::connect(
    //     repeater,
    //     SIGNAL(itemRemoved(int, QQuickItem *)),
    //     camera_model,
    //     SLOT(onItemRemoved(int, QQuickItem *))
    // );

    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });

    return app.exec();
}