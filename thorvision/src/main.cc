#include <spdlog/spdlog.h>

#include <filesystem>

#ifdef _WIN32
#include <QQuickStyle>
#endif
#include <QQuickWindow>
#include <QtGui>
#include <QtQml>
#include <cassert>
#include <csignal>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "HttpServer.h"
#include "Profiles.h"
#include "Recorder.h"
#include "RecorderSettings.h"
#include "Server.h"
#include "WebSocketClient.h"

using json = nlohmann::json;

void setup_gst_plugin_path(const QCoreApplication &app)
{
#ifdef _WIN32
    auto gst_plugin_dir =
        fmt::format("{}/../plugins/gstreamer", app.applicationDirPath().toStdString());
    auto const default_plugin_dir =
        "C:\\Program Files\\gstreamer\\1.0\\msvc_x86_64\\lib\\gstreamer-1.0";
#elif __APPLE__
    auto gst_plugin_dir =
        fmt::format("{}/../PlugIns/gstreamer", app.applicationDirPath().toStdString());
    auto const default_plugin_dir =
        "/Library/Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0";
#endif

    if (std::filesystem::exists(gst_plugin_dir)) {
        spdlog::info("set GST_PLUGIN_PATH to: {}", gst_plugin_dir);
        g_setenv("GST_PLUGIN_PATH", gst_plugin_dir.c_str(), true);
        return;
    }

    auto const env_path = g_getenv("GST_PLUGIN_PATH");
    if (env_path && *env_path) {
        spdlog::info("Set GST_PLUGIN_PATH to: {}", env_path);
        g_setenv("GST_PLUGIN_PATH", env_path, true);
        return;
    }

    spdlog::info("Set GST_PLUGIN_PATH to default: {}", default_plugin_dir);
    g_setenv("GST_PLUGIN_PATH", default_plugin_dir, true);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    setup_gst_plugin_path(app);
    gst_init(&argc, &argv);

#ifdef _WIN32
    QQuickStyle::setStyle("Fusion");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    // register GstD3D11Qt6VideoItem as qml element
    if (auto sink = gst_element_factory_make("qml6d3d11sink", nullptr)) {
        gst_object_unref(sink);
    }
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    // register GstGLQt6VideoItem as qml element
    if (auto sink = gst_element_factory_make("qml6glsink", nullptr)) {
        gst_object_unref(sink);
    }
#endif

    auto loop = g_main_loop_new(nullptr, false);

    QQmlApplicationEngine engine;

    auto camera_model = new CameraModel(&app);
    auto recorder_settings = new RecorderSettings(&app);
    auto recorder = new Recorder(camera_model, recorder_settings, &app);
    auto server = new Server(&app);
    auto ws_client = new WebSocketClient(&app);
    auto profiles = new Profiles(recorder_settings, camera_model, &app);
    HttpServer http_server(recorder);

    const auto &root_context = engine.rootContext();
    root_context->setContextProperty("CameraModel", camera_model);
    root_context->setContextProperty("Recorder", recorder);
    root_context->setContextProperty("RecorderSettings", recorder_settings);
    root_context->setContextProperty("Server", server);
    root_context->setContextProperty("Profiles", profiles);

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
            if (config->has_default_config()) {
                config->load_default();
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
            const auto &camera = Camera::parse(camera_json);
            camera_model->add_camera(camera);
        }
    );
    QObject::connect(
        ws_client,
        &WebSocketClient::camera_removed,
        camera_model,
        [camera_model, recorder](const int id) {
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
    assert(root_object && "[qml] Could not find qml root object");

    auto video_layout = root_object->findChild<QQuickItem *>("video_layout");
    assert(video_layout && "[qml] Could not find video_layout");

    auto repeater = video_layout->findChild<QQuickItem *>("repeater");
    assert(repeater && "[qml] Could not find repeater");

    QObject::connect(
        repeater,
        SIGNAL(itemAdded(int, QQuickItem *)),
        camera_model,
        SLOT(onItemAdded(int, QQuickItem *))
    );

    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });

    std::jthread gst_thread([loop]() {
        spdlog::debug("Run g_main_loop thread");
        g_main_loop_run(loop);
        spdlog::debug("Quit g_main_loop thread");
        g_main_loop_unref(loop);
    });

    auto result = app.exec();
    g_main_loop_quit(loop);

    return result;
}