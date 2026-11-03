// Live check of ExtensionClient against a device analytics service (no GUI).
//
//   extension-client-smoke --api-transport-test      (local fixture only)
//   extension-client-smoke --api-only                (read-only device check)
//   extension-client-smoke --lifecycle <camera id> <local recorded segment>
//
// Lifecycle mode is an explicit developer integration test: it changes analytics activation.
// It requires a developer-deployed API at THORVISION_DEVICE_HOST/THORVISION_DEVICE_PORT,
// a test-source encoder, a tested package named "box", and a local recording of that
// stream with frame references matching the live results. Segment transfer is outside
// this test. No remote shell or device-deployment operation is performed here.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWebSocketServer>
#include <cstdio>
#include <functional>
#include <memory>

#include "../src/CameraModel.h"
#include "../src/AgentBridgeClient.h"
#include "../src/ExtensionClient.h"
#include "xdaqvc/analytics.h"

namespace
{
int failures = 0;

void check(bool condition, const char *what)
{
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
    std::fflush(stdout);
    if (!condition) ++failures;
}

bool wait_for(const std::function<bool()> &condition, int timeout_ms)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeout_ms) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return true;
}

QString run_state(ExtensionClient &client, const QString &camera)
{
    return client.camera(camera).value("run").toMap().value("state").toString();
}

int api_transport_test()
{
    QTcpServer http;
    QWebSocketServer events("analytics API fixture", QWebSocketServer::NonSecureMode);
    check(http.listen(QHostAddress::LocalHost, 0), "local API fixture listens");
    if (!http.isListening()) return 1;
    QStringList requests;
    int event_connections = 0;
    bool available = true;
    QObject::connect(&events, &QWebSocketServer::newConnection, [&] {
        auto socket = events.nextPendingConnection();
        ++event_connections;
        QObject::connect(socket, &QWebSocket::disconnected, socket, &QObject::deleteLater);
    });
    QObject::connect(&http, &QTcpServer::newConnection, [&] {
        while (http.hasPendingConnections()) {
            auto socket = http.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const auto pending = socket->peek(socket->bytesAvailable());
                if (!pending.contains("\r\n\r\n")) return;
                const auto parts = pending.split(' ');
                if (parts.size() < 2) return;
                const auto path = QString::fromUtf8(parts[1]).section('?', 0, 0);
                requests.append(path);
                if (pending.toLower().contains("upgrade: websocket")) {
                    socket->disconnect(socket, &QTcpSocket::readyRead, nullptr, nullptr);
                    events.handleConnection(socket);
                    return;
                }
                socket->readAll();
                QByteArray body;
                int status = available ? 200 : 503;
                if (path == "/analytics/v1/capabilities") body = R"({"sdk":"0.1.0"})";
                else if (path == "/analytics/v1/packages")
                    body = R"({"packages":[{"digest":"fixture","ready":true,"can_delete":false}]})";
                else if (path == "/analytics/v1/state") body = R"({"cameras":{}})";
                else { status = 404; body = "{}"; }
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " " +
                              (status == 200 ? "OK" : "Unavailable") + "\r\nContent-Type: application/json\r\n" +
                              "Connection: close\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\n\r\n" + body);
                socket->disconnectFromHost();
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            });
        }
    });

    // Old installations may still carry these variables. They must never enable
    // an alternate transport or expose credentials in the product anymore.
    qputenv("THORVISION_ANALYTICS_TRANSPORT", "ssh");
    qputenv("THORVISION_DEVICE_SSH_USER", "obsolete-user");
    qputenv("THORVISION_EXTENSIONS_PORT", "1");
    qputenv("THORVISION_DEVICE_HOST", "127.0.0.1");
    qputenv("THORVISION_DEVICE_PORT", QByteArray::number(http.serverPort()));
    qputenv("THORVISION_ANALYTICS_AUTOCONNECT", "1");
    CameraModel cameras;
    ExtensionClient client(cameras);
    int package_updates = 0, state_updates = 0;
    QObject::connect(&client, &ExtensionClient::packages_changed, [&] { ++package_updates; });
    QObject::connect(&client, &ExtensionClient::state_changed, [&] { ++state_updates; });
    check(client.metaObject()->indexOfProperty("ssh_user") == -1 &&
              client.metaObject()->indexOfProperty("remote_port") == -1,
          "product exposes no SSH account or tunnel-port settings");
    check(wait_for([&] { return client.connected() && !client.packages().isEmpty() && event_connections > 0 &&
                               package_updates > 0 && state_updates > 0; }, 5000),
          "automatic API connection, package catalog and WebSocket events work with obsolete SSH settings");
    check(client.extensions_url() == QString("http://127.0.0.1:%1/analytics").arg(http.serverPort()),
          "all analytics traffic uses the configured device API gateway");
    check(client.connection_detail().contains("device API"), "connection describes API access");
    client.preparation_status("operation in progress", true);
    check(client.busy(), "pending local operation is visible before disconnect");
    client.disconnect_device();
    check(!client.connected() && client.extensions_url().isEmpty(), "explicit disconnect clears connection");
    check(!client.busy() && client.message().contains("Check the operation status"),
          "disconnect clears a pending UI operation without claiming device cancellation");

    available = false;
    client.connect_device();
    check(wait_for([&] { return client.connection_state() == "Service unavailable"; }, 15000),
          "missing API produces an actionable unavailable state");
    check(client.connection_detail().contains("administrator") &&
              !client.connection_detail().contains("SSH", Qt::CaseInsensitive),
          "service failure directs the user to the device administrator, without a shell-login prompt");
    available = true;
    check(wait_for([&] { return client.connected() && event_connections > 1 &&
                               package_updates > 1 && state_updates > 1; }, 8000),
          "automatic API retry recovers when the developer-deployed service returns");
    client.disconnect_device();
    const auto request_count = requests.size();
    wait_for([] { return false; }, 1200);
    check(requests.size() == request_count, "explicit disconnect stops API polling and retry");
    std::printf("%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
}  // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    if (argc == 2 && QString(argv[1]) == "--api-transport-test") return api_transport_test();
    if (argc == 2 && QString(argv[1]) == "--api-only") {
        CameraModel cameras;
        ExtensionClient client(cameras);
        check(wait_for([&] { return client.connected(); }, 30000), "analytics connects automatically through the device API");
        check(client.extensions_url().endsWith("/analytics"), "uses the existing device API gateway");
        check(wait_for([&] { return !client.packages().isEmpty(); }, 10000), "package catalog loads automatically");
        for (const auto &package : client.packages()) {
            check(package.toMap().contains("ready") && package.toMap().contains("can_delete"),
                  "catalog reports current readiness and deletion protection");
        }
        std::printf("%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
        return failures ? 1 : 0;
    }
    if (argc == 2 && QString(argv[1]) == "--identity-only") {
        // Local regression: hotplug may create the QML item before a recording snapshot
        // supplies its stable identity. No stream, device connection or agent is started.
        CameraItem camera(std::make_unique<Camera>(0, "/dev/mock-camera", "Identity fixture"));
        int identity_changes = 0;
        QObject::connect(&camera, &CameraItem::stable_id_changed, [&] { ++identity_changes; });
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("identityCamera", &camera);
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { property string observedIdentity: identityCamera.stable_id }", QUrl());
        std::unique_ptr<QObject> bound(component.create());
        check(bool(bound), "camera identity QML binding created");
        if (!bound) return 1;
        check(bound->property("observedIdentity").toString().isEmpty(), "identity starts empty before adoption");

        QVariantMap state{{"stable_id", "fixture-camera-a"}, {"srt_port", 17001}};
        camera.adopt_profile(state);
        check(camera.stable_id() == "fixture-camera-a" &&
                  bound->property("observedIdentity").toString() == "fixture-camera-a" && identity_changes == 1,
              "identity-only snapshot updates the QML binding");
        camera.adopt_profile(state);
        check(identity_changes == 1, "unchanged snapshot does not notify again");
        state["srt_port"] = 17002;
        camera.adopt_profile(state);
        check(identity_changes == 1, "port-only change does not notify identity");
        state["stable_id"] = "fixture-camera-b";
        camera.adopt_profile(state);
        check(bound->property("observedIdentity").toString() == "fixture-camera-b" && identity_changes == 2,
              "replacement identity updates the same QML item");
        std::printf("%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
        return failures ? 1 : 0;
    }
    if (argc == 2 && QString(argv[1]) == "--agents-only") {
        // Agent bridge through QProcess: discovery and account status only; no prompts, no usage.
        CameraModel cameras;
        ExtensionClient extensions(cameras);
        AgentBridgeClient agents(extensions);
        agents.start();
        check(wait_for([&] { return agents.state() == "Ready" && agents.agents().size() == 3; }, 30000),
              "bridge started and discovered three agents");
        std::printf("     %s: %s\n", qPrintable(agents.state()), qPrintable(agents.detail()));
        check(wait_for([&] { return agents.statuses().size() == 3; }, 90000), "account status for every agent");
        for (auto &id : agents.statuses().keys()) {
            auto status = agents.statuses().value(id).toMap();
            std::printf("     %s: %s, billing %s\n", qPrintable(id), qPrintable(status.value("state").toString()),
                        qPrintable(status.value("billing").toString()));
            check(QStringList{"connected", "signin_required", "quota_exhausted", "unsupported"}.contains(
                      status.value("state").toString()),
                  "status is an actionable connection state");
        }
        agents.send("hello");
        check(agents.transcript().last().toMap().value("kind") == "error", "prompts need a camera and the device connection");
        std::printf("%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
        return failures ? 1 : 0;
    }
    if (argc != 4 || QString(argv[1]) != "--lifecycle") {
        std::printf("usage: extension-client-smoke --api-transport-test | --api-only | --identity-only | --agents-only\n"
                    "       extension-client-smoke --lifecycle <camera id> <local recorded segment>\n");
        return 2;
    }
    qputenv("THORVISION_ANALYTICS_AUTOCONNECT", "0");
    const QString camera = argv[2], segment = argv[3];
    if (!QFile::exists(segment)) {
        std::printf("Provide an existing local recording from the current test stream.\n");
        return 2;
    }
    CameraModel cameras;
    ExtensionClient client(cameras);

    client.connect_device();
    check(wait_for([&] { return client.connected(); }, 20000), "device analytics API connected");
    if (!client.connected()) {
        std::printf("state=%s detail=%s\n", qPrintable(client.connection_state()),
                    qPrintable(client.connection_detail()));
        return 1;
    }
    std::printf("     sandbox %s\n",
                QJsonDocument::fromVariant(client.capabilities().value("sandbox")).toJson(QJsonDocument::Compact).constData());

    QString digest;
    check(wait_for([&] {
              for (auto &value : client.packages()) {
                  auto item = value.toMap();
                  if (item.value("manifest").toMap().value("name") == "box" &&
                      item.value("ready").toBool())
                      digest = item.value("digest").toString();
              }
              return !digest.isEmpty();
          }, 10000),
          "tested package listed");

    client.watch_preview(camera, true);
    client.apply(camera, digest);
    check(wait_for([&] { return run_state(client, camera) == "running"; }, 30000), "Apply starts the run");
    std::printf("     message: %s\n", qPrintable(client.message()));

    int updates = 0;
    QObject::connect(&client, &ExtensionClient::preview_updated, [&](const QString &id) {
        if (id == camera) ++updates;
    });
    check(wait_for([&] { return updates >= 15 && client.preview_fresh(camera); }, 20000),
          "processed preview frames arrive and are fresh");
    auto image = client.preview(camera);
    auto header = client.preview_header(camera);
    std::printf("     preview %dx%d frame %s\n", image.width(), image.height(),
                qPrintable(header.value("frame").toMap().value("sequence").toString()));
    check(!image.isNull() && header.value("frame").toMap().value("camera") == camera,
          "preview carries the frame reference of this camera");

    // PC sidecar: the developer supplies a local segment from the same test stream.
    wait_for([] { return false; }, 7000);  // Keep processing events: results keep arriving.
    QTemporaryDir directory;
    if (qEnvironmentVariableIsSet("THORVISION_SMOKE_KEEP")) {
        directory.setAutoRemove(false);
        std::printf("     keeping %s\n", qPrintable(directory.path()));
    }
    auto local = directory.filePath("pc-segment.mkv");
    check(QFile::copy(segment, local), "local recorded segment copied into the test workspace");
    std::printf("     %d results cached; segment %s\n", client.cached_results(camera), qPrintable(segment));
    {
        auto index = xvc::analytics::index_mkv(std::filesystem::path(local.toStdWString()));
        auto live = client.preview_header(camera).value("frame").toMap();
        std::printf("     segment %s: %zu frames, %s\n", index.codec.c_str(), index.frames.size(),
                    index.frames.empty() || !index.frames.front().embedded
                        ? "no frame references"
                        : (index.frames.front().embedded->ref.epoch + " " +
                           std::to_string(index.frames.front().embedded->ref.sequence) + "-" +
                           std::to_string(index.frames.back().embedded->ref.sequence))
                              .c_str());
        std::printf("     live preview %s %s\n", qPrintable(live.value("epoch").toString()),
                    qPrintable(live.value("sequence").toString()));
        for (auto &problem : index.problems) std::printf("     problem: %s\n", problem.c_str());
    }
    bool written = false, complete = false;
    QObject::connect(&client, &ExtensionClient::sidecar_written, [&](const QString &, bool w, bool c) {
        written = w;
        complete = c;
    });
    wait_for([] { return false; }, 5000);  // The client's grace period after a segment closes.
    client.write_pc_sidecar(camera, local);
    check(wait_for([&] { return written; }, 15000), "PC sidecar written from the live result cache");
    QFile manifest(directory.filePath(".analytics/pc-segment.mkv.bundle.json"));
    manifest.open(QIODevice::ReadOnly);
    auto coverage = QJsonDocument::fromJson(manifest.readAll()).object().value("coverage").toObject();
    std::printf("     coverage %s\n", QJsonDocument(coverage).toJson(QJsonDocument::Compact).constData());
    check(coverage.value("counts").toObject().value("processed_objects").toInt() >= 5, "sidecar matched results");
    check(complete && coverage.value("complete").toBool(), "sidecar coverage complete");

    client.stop(camera);
    check(wait_for([&] { return client.camera(camera).value("run").isNull(); }, 15000), "Stop ends the run");
    check(wait_for([&] { return !client.preview_fresh(camera); }, 5000), "preview goes stale (raw fallback)");
    check(client.camera(camera).value("previous").toMap().value("digest") == digest, "previous kept for Roll Back");

    client.roll_back(camera);
    check(wait_for([&] { return run_state(client, camera) == "running" && client.preview_fresh(camera); }, 30000),
          "Roll Back restores the previous package");
    std::printf("     message: %s\n", qPrintable(client.message()));
    client.stop(camera);
    wait_for([&] { return client.camera(camera).value("run").isNull(); }, 15000);

    client.disconnect_device();
    check(client.connection_state() == "Disconnected", "disconnect");
    std::printf("%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
