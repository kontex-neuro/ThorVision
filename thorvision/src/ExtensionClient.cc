#include "ExtensionClient.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QUuid>
#include <QSignalBlocker>
#include <QtEndian>
#include <QtConcurrent/QtConcurrentRun>
#include <spdlog/spdlog.h>

#include "CameraItem.h"
#include "CameraModel.h"
#include "xdaqvc/analytics.h"
#include "xdaqvc/recording.h"

namespace
{
constexpr qint64 result_cache_ms = 60'000;
constexpr std::size_t result_cache_limit = 3000;
constexpr qint64 preview_stale_ms = 2000;
constexpr int sidecar_grace_ms = 5000;

qint64 now_ms() { return QDateTime::currentMSecsSinceEpoch(); }

QString ssh_executable()
{
    auto found = QStandardPaths::findExecutable("ssh");
#ifdef _WIN32
    if (found.isEmpty() && QFileInfo::exists("C:/Windows/System32/OpenSSH/ssh.exe"))
        found = "C:/Windows/System32/OpenSSH/ssh.exe";
#endif
    return found;
}

int free_local_port()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 0;
    auto port = server.serverPort();
    server.close();
    return port;
}

// Turns OpenSSH's stderr into an actionable connection state.
std::pair<QString, QString> classify_ssh_failure(const QString &errors)
{
    if (errors.contains("REMOTE HOST IDENTIFICATION HAS CHANGED") || errors.contains("Host key verification failed"))
        return {"Host key not trusted",
                QObject::tr("The device host key is unknown or changed. Verify the device and add its key to "
                            "known_hosts (for example by running ssh once from a terminal); ThorVision never "
                            "accepts host keys automatically.")};
    if (errors.contains("Permission denied"))
        return {"SSH key required", QObject::tr("SSH key authentication was refused. Load a key for this device "
                                                "user into ssh-agent or ~/.ssh; passwords are not used here.")};
    if (errors.contains("Could not resolve") || errors.contains("timed out") || errors.contains("No route") ||
        errors.contains("Connection refused"))
        return {"Device unreachable", errors.trimmed().section('\n', -1)};
    return {"Tunnel failed", errors.trimmed().section('\n', -1)};
}
}  // namespace

ExtensionClient::ExtensionClient(CameraModel &cameras, QObject *parent) : QObject(parent), _cameras(cameras)
{
    QSettings settings;
    _ssh_user = settings.value("extensions/ssh_user", qEnvironmentVariable("THORVISION_DEVICE_SSH_USER")).toString();
    _remote_port = settings.value("extensions/remote_port", 8770).toInt();
    if (auto port = qEnvironmentVariableIntValue("THORVISION_EXTENSIONS_PORT"); port > 0 && port <= 65535)
        _remote_port = port;

    _tunnel.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&_tunnel, &QProcess::readyReadStandardError, this, [this] {
        _tunnel_errors = (_tunnel_errors + QString::fromLocal8Bit(_tunnel.readAllStandardError())).right(4096);
    });
    connect(&_tunnel, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (!_wanted) return;
        auto [state, detail] = classify_ssh_failure(_tunnel_errors);
        spdlog::warn("Extension tunnel exited ({}): {}", code, _tunnel_errors.toStdString());
        teardown();
        set_connection(state, detail);
        // Retry transient failures only; trust and credential problems need the user.
        if (state == "Device unreachable" || state == "Tunnel failed") _reconnect.start(5000);
    });
    connect(&_tunnel, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            set_connection("Tunnel failed", tr("OpenSSH (ssh) could not be started."));
    });

    _reconnect.setSingleShot(true);
    connect(&_reconnect, &QTimer::timeout, this, [this] {
        if (_wanted) start_tunnel();
    });
    _poll.setInterval(1000);
    connect(&_poll, &QTimer::timeout, this, &ExtensionClient::poll_state);
    _freshness.setInterval(500);
    connect(&_freshness, &QTimer::timeout, this, &ExtensionClient::update_freshness);
    _freshness.start();

    connect(&_events, &QWebSocket::textMessageReceived, this, &ExtensionClient::on_event);
    connect(&_events, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        spdlog::warn("Extension event stream: {}", _events.errorString().toStdString());
    });
    connect(&_events, &QWebSocket::disconnected, this, [this] {
        if (connected()) QTimer::singleShot(1000, this, &ExtensionClient::open_events);
    });

    connect(&_cameras, &CameraModel::camera_count_changed, this, &ExtensionClient::hook_cameras);
    hook_cameras();
}

ExtensionClient::~ExtensionClient()
{
    _wanted = false;
    teardown();
}

QString ExtensionClient::device_host() const { return QString::fromStdString(xvc::device_host()); }

void ExtensionClient::set_ssh_user(const QString &user)
{
    if (user == _ssh_user) return;
    _ssh_user = user.trimmed();
    QSettings().setValue("extensions/ssh_user", _ssh_user);
    emit settings_changed();
}

void ExtensionClient::set_remote_port(int port)
{
    if (port == _remote_port || port <= 0 || port > 65535) return;
    _remote_port = port;
    QSettings().setValue("extensions/remote_port", port);
    emit settings_changed();
}

QUrl ExtensionClient::url(const QString &path, const QString &scheme) const
{
    return QUrl::fromEncoded(QString("%1://127.0.0.1:%2%3").arg(scheme).arg(_local_port).arg(path).toUtf8());
}

void ExtensionClient::set_connection(const QString &state, const QString &detail)
{
    if (state == _connection_state && detail == _connection_detail) return;
    _connection_state = state;
    _connection_detail = detail;
    emit connection_changed();
}

void ExtensionClient::set_message(const QString &message, bool busy)
{
    _message = message;
    _busy = busy;
    emit message_changed();
}

void ExtensionClient::connect_device()
{
    if (_ssh_user.isEmpty()) {
        set_connection("Disconnected", tr("Enter the SSH user of the device account."));
        return;
    }
    _wanted = true;
    start_tunnel();
}

void ExtensionClient::disconnect_device()
{
    _wanted = false;
    _reconnect.stop();
    teardown();
    set_connection("Disconnected");
}

void ExtensionClient::teardown()
{
    _poll.stop();
    _events.abort();
    for (auto &[camera, preview] : _previews)
        if (preview.socket) preview.socket->abort();
    if (_tunnel.state() != QProcess::NotRunning) {
        // A deliberate stop is not a tunnel failure.
        QSignalBlocker blocker(&_tunnel);
        _tunnel.kill();
        _tunnel.waitForFinished(2000);
    }
}

void ExtensionClient::start_tunnel()
{
    teardown();
    auto ssh = ssh_executable();
    if (ssh.isEmpty()) {
        set_connection("Tunnel failed", tr("OpenSSH (ssh) was not found on this PC."));
        return;
    }
    _local_port = free_local_port();
    if (!_local_port) {
        set_connection("Tunnel failed", tr("No local port is available for the tunnel."));
        return;
    }
    _tunnel_errors.clear();
    _capability_attempts = 0;
    set_connection("Connecting", tr("Opening SSH tunnel to %1").arg(device_host()));
    _tunnel.start(ssh, {"-N", "-T", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes", "-o",
                        "ExitOnForwardFailure=yes", "-o", "ServerAliveInterval=5", "-o", "ServerAliveCountMax=3",
                        "-o", "ConnectTimeout=8", "-L",
                        QString("127.0.0.1:%1:127.0.0.1:%2").arg(_local_port).arg(_remote_port),
                        QString("%1@%2").arg(_ssh_user, device_host())});
    QTimer::singleShot(600, this, &ExtensionClient::check_service);
}

void ExtensionClient::check_service()
{
    if (!_wanted || _tunnel.state() != QProcess::Running) return;
    request("GET", "/v1/capabilities", {}, [this](int status, const QJsonDocument &body) {
        if (status == 200) {
            _capabilities = body.object().toVariantMap();
            _failed_polls = 0;
            set_connection("Connected", tr("%1 via SSH (%2)").arg(device_host(), _ssh_user));
            open_events();
            for (auto &[camera, preview] : _previews)
                if (preview.watchers) open_preview(camera, preview);
            refresh_packages();
            poll_state();
            _poll.start();
            return;
        }
        if (++_capability_attempts < 20) {
            QTimer::singleShot(500, this, &ExtensionClient::check_service);
            return;
        }
        set_connection("Service unavailable", tr("The tunnel is open but the analytics service does not respond "
                                                 "on device port %1.").arg(_remote_port));
    });
}

void ExtensionClient::request(const QByteArray &method, const QString &path, const QByteArray &body, Reply done,
                              const QByteArray &idempotency_key)
{
    QNetworkRequest request(url(path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(method == "POST" && path == "/v1/packages" ? 60000 : 8000);
    if (!idempotency_key.isEmpty()) request.setRawHeader("Idempotency-Key", idempotency_key);
    auto reply = _network.sendCustomRequest(request, method, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(status, QJsonDocument::fromJson(reply->readAll()));
    });
}

void ExtensionClient::poll_state()
{
    request("GET", "/v1/state", {}, [this](int status, const QJsonDocument &body) {
        if (status != 200) {
            if (++_failed_polls >= 3 && connected()) {
                set_connection("Service unavailable", tr("The analytics service stopped responding."));
                if (_wanted) _reconnect.start(3000);
            }
            return;
        }
        _failed_polls = 0;
        if (!connected() && _wanted) set_connection("Connected", tr("%1 via SSH (%2)").arg(device_host(), _ssh_user));
        _state = body.object().toVariantMap();
        emit state_changed();
    });
}

void ExtensionClient::refresh_packages()
{
    request("GET", "/v1/packages", {}, [this](int status, const QJsonDocument &body) {
        if (status != 200) return;
        _packages = body.object().value("packages").toArray().toVariantList();
        emit packages_changed();
    });
}

QVariantMap ExtensionClient::camera(const QString &camera) const
{
    return _state.value("cameras").toMap().value(camera).toMap();
}

QStringList ExtensionClient::camera_ids() const
{
    QStringList ids;
    for (auto item : _cameras.cameras())
        if (item && !ids.contains(item->stable_id())) ids << item->stable_id();
    for (auto &id : _state.value("cameras").toMap().keys())
        if (!ids.contains(id)) ids << id;
    return ids;
}

void ExtensionClient::upload_package(const QUrl &archive)
{
    QFile file(archive.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        set_message(tr("Cannot read %1").arg(archive.toLocalFile()));
        return;
    }
    set_message(tr("Uploading %1").arg(QFileInfo(file).fileName()), true);
    request("POST", "/v1/packages", file.readAll(), [this](int status, const QJsonDocument &body) {
        auto object = body.object();
        if (status != 200) {
            set_message(tr("Upload rejected: %1").arg(object.value("error").toString()));
            return;
        }
        set_message(tr("Uploaded %1 (%2)").arg(object.value("manifest").toObject().value("name").toString(),
                                              object.value("digest").toString().left(12)));
        refresh_packages();
    });
}

void ExtensionClient::test_package(const QString &digest)
{
    set_message(tr("Testing %1 on the device fixture").arg(digest.left(12)), true);
    request("POST", QString("/v1/packages/%1/test").arg(digest), {}, [this](int status, const QJsonDocument &body) {
        if (status != 200) {
            set_message(tr("Test could not start: %1").arg(body.object().value("error").toString()));
            return;
        }
        watch_job(body.object().value("id").toString(), tr("Test"));
    });
}

void ExtensionClient::watch_job(const QString &job, const QString &label)
{
    request("GET", QString("/v1/jobs/%1").arg(job), {}, [this, job, label](int status, const QJsonDocument &body) {
        auto object = body.object();
        if (status == 200 && object.value("state").toString() == "running") {
            QTimer::singleShot(500, this, [this, job, label] { watch_job(job, label); });
            return;
        }
        auto report = object.value("report").toObject();
        if (object.value("state").toString() == "succeeded")
            set_message(tr("%1 passed: %2 frames, %3 ms median callback")
                            .arg(label)
                            .arg(report.value("frames").toInt())
                            .arg(report.value("callback_ms").toObject().value("median").toDouble()));
        else {
            auto fault = report.value("fault").toObject().value("message").toString();
            if (fault.isEmpty()) fault = report.value("error").toString();
            if (fault.isEmpty()) fault = object.value("error").toString();
            set_message(tr("%1 failed: %2").arg(label, fault));
        }
        refresh_packages();
    });
}

void ExtensionClient::with_fresh_state(std::function<void()> then)
{
    // Mutations carry the configuration revision they were decided on; read it fresh so a
    // change made elsewhere (or a not-yet-polled state) is refused by the device, not guessed.
    request("GET", "/v1/state", {}, [this, then = std::move(then)](int status, const QJsonDocument &body) {
        if (status != 200) {
            set_message(tr("The analytics service did not answer; nothing was changed."));
            return;
        }
        _state = body.object().toVariantMap();
        emit state_changed();
        then();
    });
}

void ExtensionClient::apply(const QString &camera, const QString &digest)
{
    set_message(tr("Applying %1").arg(digest.left(12)), true);
    with_fresh_state([this, camera, digest] {
        auto revision = this->camera(camera).value("revision", 0).toInt();
        QJsonObject body{{"digest", digest}, {"config", QJsonObject{}}, {"revision", revision}};
        auto key = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
        request("PUT", QString("/v1/cameras/%1/extension").arg(QString(QUrl::toPercentEncoding(camera))),
                QJsonDocument(body).toJson(QJsonDocument::Compact),
                [this](int status, const QJsonDocument &reply) {
                    set_message(status == 200
                                    ? tr("Applied; the processed preview starts with the next frames.")
                                    : tr("Apply refused: %1").arg(reply.object().value("error").toString()));
                    poll_state();
                },
                key);
    });
}

void ExtensionClient::stop(const QString &camera)
{
    set_message(tr("Stopping analytics"), true);
    with_fresh_state([this, camera] {
        auto revision = this->camera(camera).value("revision", 0).toInt();
        request("DELETE",
                QString("/v1/cameras/%1/extension?revision=%2")
                    .arg(QString(QUrl::toPercentEncoding(camera)))
                    .arg(revision),
                {}, [this](int status, const QJsonDocument &reply) {
                    set_message(status == 200 ? tr("Analytics stopped; raw preview restored.")
                                              : tr("Stop refused: %1").arg(reply.object().value("error").toString()));
                    poll_state();
                });
    });
}

void ExtensionClient::roll_back(const QString &camera)
{
    with_fresh_state([this, camera] {
        auto previous = this->camera(camera).value("previous").toMap().value("digest").toString();
        if (previous.isEmpty()) {
            set_message(tr("No previous version is available for this camera."));
            return;
        }
        apply(camera, previous);
    });
}

void ExtensionClient::open_events()
{
    _events.abort();
    _events.open(url(QString("/v1/events?cursor=%1").arg(_events_cursor), "ws"));
}

void ExtensionClient::on_event(const QString &text)
{
    auto event = nlohmann::json::parse(text.toStdString(), nullptr, false);
    if (event.is_discarded()) return;
    if (event.contains("cursor") && event["cursor"].is_number_integer()) _events_cursor = event["cursor"];
    auto type = event.value("type", "");
    if (type == "gap") {
        // Results were missed: frames before the next result can no longer be called "not sampled".
        _covered_from.clear();
        poll_state();
        return;
    }
    if (type == "result" && event.contains("record")) {
        auto &record = event["record"];
        auto camera = QString::fromStdString(record["frame"].value("camera", ""));
        auto epoch = record["frame"].value("epoch", "");
        auto sequence = std::stoull(record["frame"].value("sequence", "0"));
        auto &covered = _covered_from[camera];
        if (!covered.contains(epoch)) covered[epoch] = sequence;
        auto &cache = _results[camera];
        cache.push_back({now_ms(), record});
        while (!cache.empty() && (now_ms() - cache.front().received_ms > result_cache_ms ||
                                  cache.size() > result_cache_limit)) {
            auto &oldest = cache.front().record["frame"];
            auto &from = covered[oldest.value("epoch", "")];
            from = std::max<std::uint64_t>(from, std::stoull(oldest.value("sequence", "0")) + 1);
            cache.pop_front();
        }
        return;
    }
    if (type == "job" || type == "package_added") refresh_packages();
    poll_state();
}

void ExtensionClient::watch_preview(const QString &camera, bool watching)
{
    if (camera.isEmpty()) return;
    auto &preview = _previews[camera];
    preview.watchers = std::max(0, preview.watchers + (watching ? 1 : -1));
    if (preview.watchers && !preview.socket && connected()) open_preview(camera, preview);
    if (!preview.watchers && preview.socket) {
        preview.socket->abort();
        preview.socket.reset();
    }
}

void ExtensionClient::open_preview(const QString &camera, Preview &preview)
{
    preview.socket = std::make_unique<QWebSocket>();
    auto socket = preview.socket.get();
    connect(socket, &QWebSocket::binaryMessageReceived, this,
            [this, camera](const QByteArray &data) { on_preview(camera, data); });
    connect(socket, &QWebSocket::errorOccurred, this, [socket](QAbstractSocket::SocketError) {
        spdlog::warn("Extension preview stream: {}", socket->errorString().toStdString());
    });
    connect(socket, &QWebSocket::disconnected, this, [this, camera, socket] {
        QTimer::singleShot(1000, this, [this, camera, socket] {
            auto found = _previews.find(camera);
            if (found == _previews.end() || found->second.socket.get() != socket || !found->second.watchers ||
                !connected())
                return;
            open_preview(camera, found->second);
        });
    });
    socket->open(url(QString("/v1/cameras/%1/preview").arg(QString(QUrl::toPercentEncoding(camera))), "ws"));
}

void ExtensionClient::on_preview(const QString &camera, const QByteArray &data)
{
    // TVP1 | u32 little-endian header length | header JSON | JPEG
    if (data.size() < 8 || !data.startsWith("TVP1")) return;
    auto size = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 4));
    if (8 + qsizetype(size) > data.size()) return;
    auto header = QJsonDocument::fromJson(data.mid(8, size)).object().toVariantMap();
    QImage image;
    if (!image.loadFromData(data.mid(8 + size), "JPG")) return;
    auto &preview = _previews[camera];
    preview.image = std::move(image);
    preview.header = header;
    preview.received_ms = now_ms();
    emit preview_updated(camera);
    update_freshness();
}

void ExtensionClient::update_freshness()
{
    for (auto &[camera, preview] : _previews) {
        bool fresh = preview.received_ms && now_ms() - preview.received_ms < preview_stale_ms && connected();
        if (fresh == preview.fresh) continue;
        preview.fresh = fresh;
        // Never hold an old annotated image: stale previews fall back to the raw stream, whose
        // decoding is suspended only while the device-rendered preview is fresh.
        for (auto item : _cameras.cameras())
            if (item && item->stable_id() == camera) item->set_display_suspended(fresh);
        emit preview_freshness_changed(camera);
        emit preview_updated(camera);
    }
}

bool ExtensionClient::preview_fresh(const QString &camera) const
{
    auto found = _previews.find(camera);
    return found != _previews.end() && found->second.fresh;
}

QVariantMap ExtensionClient::preview_header(const QString &camera) const
{
    auto found = _previews.find(camera);
    return found == _previews.end() ? QVariantMap{} : found->second.header;
}

QImage ExtensionClient::preview(const QString &camera) const
{
    auto found = _previews.find(camera);
    return found == _previews.end() || !found->second.fresh ? QImage{} : found->second.image;
}

void ExtensionClient::hook_cameras()
{
    for (auto item : _cameras.cameras()) {
        if (!item || _hooked.contains(item)) continue;
        _hooked.insert(item, true);
        connect(item, &QObject::destroyed, this, [this, item] { _hooked.remove(item); });
        connect(item, &CameraItem::recording_fragment_closed, this,
                [this, item](const QString &path) { on_fragment_closed(item->stable_id(), path); });
    }
}

void ExtensionClient::on_fragment_closed(const QString &camera, const QString &path)
{
    // Results may still arrive shortly after the file closes; wait before matching.
    QTimer::singleShot(sidecar_grace_ms, this, [this, camera, path] { write_pc_sidecar(camera, path); });
}

void ExtensionClient::write_pc_sidecar(const QString &camera, const QString &path)
{
    {
        auto &cache = _results[camera];
        if (cache.empty()) return;  // No analytics for this camera: PC recording stays video-only.
        std::vector<nlohmann::json> records;
        for (auto &entry : cache) records.push_back(entry.record);
        auto covered = _covered_from.value(camera);
        std::vector<std::string> problems;
        if (!connected()) problems.push_back("analytics service connection was lost during PC recording");
        auto watcher = new QFutureWatcher<xvc::analytics::SegmentReport>(this);
        connect(watcher, &QFutureWatcher<xvc::analytics::SegmentReport>::finished, this, [this, watcher, path] {
            auto report = watcher->result();
            watcher->deleteLater();
            if (report.written)
                spdlog::info("PC analytics sidecar for {}: complete={}", path.toStdString(), report.complete);
            emit sidecar_written(path, report.written, report.complete);
        });
        watcher->setFuture(QtConcurrent::run([path, records = std::move(records), covered, problems] {
            try {
                return xvc::analytics::write_segment(std::filesystem::path(path.toStdWString()), records, covered,
                                                     problems);
            } catch (const std::exception &error) {
                spdlog::error("PC analytics sidecar failed for {}: {}", path.toStdString(), error.what());
                return xvc::analytics::SegmentReport{};
            }
        }));
    }
}
