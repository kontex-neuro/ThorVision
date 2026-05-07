#include <gst/gst.h>
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

#include "CameraModel.h"
#include "Config.h"
#include "HttpServer.h"
#include "Recorder.h"
#include "Server.h"
#include "WebSocketClient.h"


bool setup_gst_plugin_path(const QCoreApplication &app)
{
#ifdef _WIN32
    const auto &gst_plugin_dir =
        std::format("{}/../plugins/gstreamer", app.applicationDirPath().toStdString());
    constexpr auto &default_plugin_dir =
        "C:\\Program Files\\gstreamer\\1.0\\msvc_x86_64\\lib\\gstreamer-1.0";
#elif __APPLE__
    const auto &gst_plugin_dir =
        std::format("{}/../PlugIns/gstreamer", app.applicationDirPath().toStdString());
    constexpr auto &default_plugin_dir =
        "/Library/Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0";
#endif

    if (std::filesystem::exists(gst_plugin_dir)) {
        spdlog::info("set GST_PLUGIN_PATH to: {}", gst_plugin_dir);
        g_setenv("GST_PLUGIN_PATH", gst_plugin_dir.c_str(), true);
        return true;
    }

    const auto &env_path = g_getenv("GST_PLUGIN_PATH");
    if (env_path && *env_path) {
        spdlog::info("Set GST_PLUGIN_PATH to: {}", env_path);
        g_setenv("GST_PLUGIN_PATH", env_path, true);
        return true;
    }

    spdlog::info("Set GST_PLUGIN_PATH to default: {}", default_plugin_dir);
    g_setenv("GST_PLUGIN_PATH", default_plugin_dir, true);
    return true;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    setup_gst_plugin_path(app);
    gst_init(&argc, &argv);

    if (argc == 1) {
        spdlog::set_level(spdlog::level::info);
    } else if (argc == 2 && std::format("{}", argv[1]) == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (argc == 2 && std::format("{}", argv[1]) == "trace") {
        spdlog::set_level(spdlog::level::trace);
    }

#ifdef _WIN32
    QQuickStyle::setStyle("Fusion");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    // register GstD3D11Qt6VideoItem as qml element
    if (auto sink = gst_element_factory_make("qml6d3d11sink", nullptr)) {
        gst_object_unref(sink);
    } else {
        spdlog::critical("Failed to create element: 'qml6d3d11sink'");
        return -1;
    }
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    // register GstGLQt6VideoItem as qml element
    if (auto sink = gst_element_factory_make("qml6glsink", nullptr)) {
        gst_object_unref(sink);
    } else {
        spdlog::critical("Failed to create element: 'qml6glsink'");
        return -1;
    }
#endif

    auto loop = g_main_loop_new(nullptr, false);

    CameraModel camera_model;
    Server server;
    WebSocketClient ws_client;
    Recorder recorder(camera_model);
    Config config(recorder.settings, camera_model);
    HttpServer http_server(recorder, camera_model);

    QQmlApplicationEngine engine;

    const auto &root_context = engine.rootContext();
    root_context->setContextProperty("CameraModel", &camera_model);
    root_context->setContextProperty("Recorder", &recorder);
    root_context->setContextProperty("RecorderSettings", &recorder.settings);
    root_context->setContextProperty("Server", &server);
    root_context->setContextProperty("Config", &config);

    const QUrl url(QStringLiteral("qrc:/qt/qml/App/Theme/ui/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.load(url);
    if (engine.rootObjects().isEmpty()) return -1;

    QObject::connect(
        &server, &Server::status_change, [&camera_model, &config, &server](const auto &connected) {
            if (connected) {
                // TODO: check once or check every time connecting to server ?
                if (!server.check_api_version()) {
                    return;
                }
                for (auto &camera : Camera::cameras()) {
                    camera_model.add_camera(std::move(camera));
                }
                if (config.has_default_config()) {
                    config.load_default();
                }
            } else {
                for (auto i = camera_model.rowCount() - 1; i >= 0; --i) {
                    camera_model.remove_camera(i);
                }
            }
        }
    );
    QObject::connect(
        &ws_client,
        &WebSocketClient::camera_added,
        &camera_model,
        [&camera_model](const auto &json_text) {
            auto camera = Camera::parse(json_text);
            camera_model.add_camera(std::move(camera));
        }
    );
    QObject::connect(
        &ws_client,
        &WebSocketClient::camera_removed,
        &camera_model,
        [&camera_model, &recorder](const auto &id) {
            const auto index = camera_model.index_of_camera_id(id);
            if (index == -1) {
                spdlog::error("Camera: id {} not found", id);
                return;
            }
            camera_model.remove_camera(index);

            if (recorder.recording()) {
                const auto &model_index = camera_model.index(index);
                const auto &camera_name =
                    camera_model.data(model_index, CameraModel::NameRole).toString();
                emit camera_model.camera_unplugged_during_recording(camera_name);
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
        &camera_model,
        SLOT(onItemAdded(int, QQuickItem *))
    );

    std::signal(SIGINT, [](int) { QCoreApplication::exit(0); });

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