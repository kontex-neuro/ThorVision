#include <gst/gst.h>
#include <spdlog/spdlog.h>

#include <filesystem>

#ifdef _WIN32
#include <QQuickStyle>
#endif
#include <QQuickWindow>
#include <QThread>
#include <QtGui>
#include <QtQml>
#include <cassert>
#include <csignal>

#include "CameraModel.h"
#include "Config.h"
#include "HttpServer.h"
#include "Recorder.h"
#include "Server.h"
#include "UpdateController.h"
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
    UpdateController update_controller;

    QQmlApplicationEngine engine;

    // Exposes UpdateController::State / ::Recovery to QML as UpdateState.State.Downloading
    // etc. Registered uncreatable: the single instance comes from the context property.
    qmlRegisterUncreatableType<UpdateController>(
        "App.Theme", 0, 1, "UpdateState", "UpdateController is provided as the Update singleton"
    );

    const auto &root_context = engine.rootContext();
    root_context->setContextProperty("CameraModel", &camera_model);
    root_context->setContextProperty("Recorder", &recorder);
    root_context->setContextProperty("RecorderSettings", &recorder.settings);
    root_context->setContextProperty("Server", &server);
    root_context->setContextProperty("Config", &config);
    root_context->setContextProperty("Update", &update_controller);

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
        &server,
        &Server::status_change,
        [&camera_model, &update_controller](const auto &connected) {
            if (connected) {
                // No cameras are enumerated here. On connect the app talks to the server for
                // one purpose only -- read its version and decide whether to update -- because
                // a successful update restarts the server and would strand anything built
                // against the old one. UpdateController signals when to bring cameras up.
                update_controller.on_device_connected();
            } else {
                update_controller.on_device_disconnected();
                for (auto i = camera_model.rowCount() - 1; i >= 0; --i) {
                    camera_model.remove_camera(i);
                }
            }
        }
    );
    // The whole camera bring-up is deferred until the update question resolves -- up to date,
    // ignored, a failed check, or a finished update. When an update did run, the device has
    // rebooted by this point and the list we ask for is the one the NEW server reports.
    QObject::connect(
        &update_controller,
        &UpdateController::streams_released,
        [&camera_model, &config, &ws_client, &update_controller]() {
            // The camera-event socket is attached to the device server, which an update
            // restarts. That connection dies with it and does not come back on its own, so
            // without this every hotplug event is lost until the app is restarted. Only
            // needed when the device actually rebooted -- an up-to-date device never dropped
            // the connection. Redial before rebuilding so no event falls between the two.
            if (update_controller.device_did_restart()) {
                ws_client.reconnect();
            }

            // Rebuild from scratch: on the post-update path the model may still hold entries
            // enumerated from the server that has since restarted, and the device reassigns
            // camera ids across a restart -- a stale id makes every later hotplug event fail
            // its lookup and silently do nothing.
            for (auto i = camera_model.rowCount() - 1; i >= 0; --i) {
                camera_model.remove_camera(i);
            }

            // /cameras can still 404 for a moment after the server restarts. Retry briefly
            // rather than settling for an empty list -- this runs on the GUI thread, so the
            // budget is deliberately small.
            auto cameras = Camera::cameras();
            for (auto attempt = 0; cameras.empty() && attempt < 5; ++attempt) {
                QThread::msleep(200);
                cameras = Camera::cameras();
            }
            if (cameras.empty()) {
                spdlog::warn("No cameras reported after the update; unplug and replug to rescan");
            }
            for (auto &camera : cameras) {
                camera_model.add_camera(std::move(camera));
            }
            if (config.has_default_config()) {
                config.load_default();
            }
        }
    );
    QObject::connect(&recorder, &Recorder::recording_changed, [&recorder, &update_controller]() {
        update_controller.set_recording(recorder.recording());
    });
    QObject::connect(
        &ws_client,
        &WebSocketClient::camera_added,
        &camera_model,
        [&camera_model, &update_controller](const auto &json_text) {
            // The WebSocket is a second, independent connection to the device and knows
            // nothing about updates. While one is running the server is being torn down and
            // restarted, so its hotplug events describe a machine in flux -- acting on them
            // corrupts the model. The list is rebuilt from scratch once the update resolves.
            //
            // Deliberately NOT streams_blocked(): that is also true whenever the device is
            // absent or the question is merely unanswered, and discarding real hotplug
            // events in those states leaves the camera list permanently frozen.
            if (update_controller.device_restarting()) return;

            // Camera::parse returns null when the payload fails validation (observed: the
            // device omits device_id for some cameras). add_camera() dereferences without
            // checking, so passing that through is undefined behaviour.
            auto camera = Camera::parse(json_text);
            if (!camera) {
                spdlog::error("Ignoring hotplug event: camera payload could not be parsed");
                return;
            }
            camera_model.add_camera(std::move(camera));
        }
    );
    QObject::connect(
        &ws_client,
        &WebSocketClient::camera_removed,
        &camera_model,
        [&camera_model, &recorder, &update_controller](const auto &id) {
            // See camera_added: hotplug events during an update describe a server that is
            // restarting, not a camera the user unplugged.
            if (update_controller.device_restarting()) return;

            const auto index = camera_model.index_of_camera_id(id);
            if (index == -1) {
                // The device reassigns camera ids when its server restarts, so after an
                // update the model can hold ids the device no longer uses. Silently ignoring
                // this leaves the list frozen: every subsequent unplug misses too. Drop the
                // stale entries and let the device re-announce what is actually attached.
                spdlog::warn("Camera id {} not in model; resyncing camera list", id);
                for (auto i = camera_model.rowCount() - 1; i >= 0; --i) {
                    camera_model.remove_camera(i);
                }
                for (auto &camera : Camera::cameras()) {
                    camera_model.add_camera(std::move(camera));
                }
                return;
            }

            // Read the name BEFORE removing the row -- afterwards the index refers to a
            // different camera, or to nothing at all.
            const auto camera_name =
                camera_model.data(camera_model.index(index), CameraModel::NameRole).toString();

            camera_model.remove_camera(index);

            if (recorder.recording()) {
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