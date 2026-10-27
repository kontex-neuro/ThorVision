// Live check of ExtensionClient against a device analytics service (no GUI).
//
//   extension-client-smoke <ssh user> <camera id> <device segment directory>
//
// Expects THORVISION_DEVICE_HOST and THORVISION_EXTENSIONS_PORT, a running test-source encoder
// for <camera id>, a loop_recorder writing that stream to <device segment directory>, and a
// tested package named "box" on the service. Exercises tunnel, Apply, processed preview, stale
// detection, Stop, Roll Back, the PC result cache/sidecar and a refused SSH user.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <functional>

#include "../src/CameraModel.h"
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
}  // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    if (argc != 4) {
        std::printf("usage: extension-client-smoke <ssh user> <camera id> <device segment directory>\n");
        return 2;
    }
    const QString user = argv[1], camera = argv[2], segments = argv[3];
    CameraModel cameras;
    ExtensionClient client(cameras);

    client.set_ssh_user("nobody-" + user);
    client.connect_device();
    check(wait_for([&] { return client.connection_state() == "SSH key required"; }, 20000),
          "refused SSH user is reported as SSH key required (no password prompt)");
    client.disconnect_device();

    client.set_ssh_user(user);
    client.connect_device();
    check(wait_for([&] { return client.connected(); }, 20000), "tunnel and service connected");
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
                      item.value("report").toMap().value("passed").toBool())
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

    // PC sidecar: fetch a segment the device recorded from the same stream after results were cached.
    wait_for([] { return false; }, 7000);  // Keep processing events: results keep arriving.
    QTemporaryDir directory;
    if (qEnvironmentVariableIsSet("THORVISION_SMOKE_KEEP")) {
        directory.setAutoRemove(false);
        std::printf("     keeping %s\n", qPrintable(directory.path()));
    }
    QProcess list;
    list.start("ssh", {"-o", "BatchMode=yes", QString("%1@%2").arg(user, client.device_host()),
                       QString("ls -1t %1/*.mkv* | sed -n 2p").arg(segments)});
    list.waitForFinished(15000);
    auto remote = QString::fromUtf8(list.readAllStandardOutput()).trimmed();
    auto local = directory.filePath("pc-segment.mkv");
    QProcess copy;
    copy.start("scp", {"-q", "-o", "BatchMode=yes", QString("%1@%2:%3").arg(user, client.device_host(), remote), local});
    copy.waitForFinished(30000);
    check(QFile::exists(local), "recorded segment fetched");
    std::printf("     %d results cached; segment %s\n", client.cached_results(camera), qPrintable(remote));
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
