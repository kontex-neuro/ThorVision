#include "ExtensionClient.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QUuid>
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

}  // namespace

ExtensionClient::ExtensionClient(CameraModel &cameras, QObject *parent) : QObject(parent), _cameras(cameras)
{
    _reconnect.setSingleShot(true);
    connect(&_reconnect, &QTimer::timeout, this, [this] {
        if (_wanted) connect_api();
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
        if (!_wanted || !connected()) return;
        const auto generation = _connection_generation;
        QTimer::singleShot(1000, this, [this, generation] {
            if (generation == _connection_generation && _wanted && connected() &&
                _events.state() == QAbstractSocket::UnconnectedState)
                open_events();
        });
    });

    connect(&_cameras, &CameraModel::camera_count_changed, this, &ExtensionClient::hook_cameras);
    hook_cameras();
    const auto auto_connect = qEnvironmentVariableIsSet("THORVISION_FRAMELAB_AUTOCONNECT")
        ? qEnvironmentVariable("THORVISION_FRAMELAB_AUTOCONNECT")
        : qEnvironmentVariable("THORVISION_ANALYTICS_AUTOCONNECT");
    if (auto_connect != "0")
        QTimer::singleShot(0, this, &ExtensionClient::connect_device);
}

ExtensionClient::~ExtensionClient()
{
    _wanted = false;
    teardown();
}

QString ExtensionClient::device_host() const { return QString::fromStdString(xvc::device_host()); }

QUrl ExtensionClient::url(const QString &path, const QString &scheme) const
{
    auto base = QUrl(_base_url);
    if (scheme == "ws") base.setScheme(base.scheme() == "https" ? "wss" : "ws");
    return QUrl::fromEncoded(base.toEncoded() + path.toUtf8());
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
    _wanted = true;
    connect_api();
}

void ExtensionClient::disconnect_device()
{
    _wanted = false;
    _reconnect.stop();
    set_connection("Disconnected");
    teardown();
}

void ExtensionClient::teardown()
{
    ++_connection_generation;
    _poll.stop();
    _events.abort();
    for (auto &[camera, preview] : _previews)
        if (preview.socket) preview.socket->abort();
    if (_busy)
        set_message(tr("The device connection was interrupted. Check the operation status after reconnecting."));
}

void ExtensionClient::connect_api()
{
    _reconnect.stop();
    teardown();
    _capability_attempts = 0;
    _base_url = QString::fromStdString(xvc::device_url()) + "/framelab";
    set_connection("Connecting", tr("Connecting to FrameLab on %1…").arg(device_host()));
    check_service();
}

void ExtensionClient::check_service()
{
    if (!_wanted) return;
    request("GET", "/v1/capabilities", {}, [this](int status, const QJsonDocument &body) {
        if (status == 200) {
            _capabilities = body.object().toVariantMap();
            _failed_polls = 0;
            set_connection("Connected", tr("Connected automatically through the device API."));
            open_events();
            for (auto &[camera, preview] : _previews)
                if (preview.watchers) open_preview(camera, preview);
            refresh_packages();
            poll_state();
            _poll.start();
            return;
        }
        // Discover legacy devices using a read-only probe; never replay mutations.
        if (status == 404 && _base_url.endsWith("/framelab")) {
            _base_url = QString::fromStdString(xvc::device_url()) + "/analytics";
            check_service();
            return;
        }
        if (++_capability_attempts < 20) {
            const auto generation = _connection_generation;
            QTimer::singleShot(500, this, [this, generation] {
                if (generation == _connection_generation) check_service();
            });
            return;
        }
        set_connection("Service unavailable", tr("The device FrameLab API is unavailable. Check the network or contact the device administrator to deploy or restore the service. Retrying automatically."));
        if (_wanted) _reconnect.start(5000);
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
    auto generation = _connection_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, done = std::move(done)] {
        reply->deleteLater();
        if (generation != _connection_generation) return;
        auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(status, QJsonDocument::fromJson(reply->readAll()));
    });
}

void ExtensionClient::poll_state()
{
    request("GET", "/v1/state", {}, [this](int status, const QJsonDocument &body) {
        if (status != 200) {
            if (++_failed_polls >= 3 && connected()) {
                set_connection("Service unavailable", tr("The FrameLab service stopped responding."));
                if (_wanted) _reconnect.start(3000);
            }
            return;
        }
        _failed_polls = 0;
        if (!connected() && _wanted) set_connection("Connected", tr("Connected automatically through the device API."));
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
        emit prepare_requested(object.value("digest").toString());
    });
}

void ExtensionClient::preparation_status(const QString &message, bool busy)
{
    set_message(message, busy);
    if (!busy) refresh_packages();
}

void ExtensionClient::plan_cleanup(const QStringList &digests)
{
    if (!connected() || _busy) return;
    _cleanup_plan.clear();
    set_message(tr("Checking unused packages and shared files…"), true);
    QJsonObject body{{"packages", QJsonArray::fromStringList(digests)}, {"include_unused_cache", true}};
    request("POST", "/v1/cleanup/plan", QJsonDocument(body).toJson(QJsonDocument::Compact),
            [this](int status, const QJsonDocument &reply) {
                if (status != 200) {
                    set_message(tr("Cleanup preview failed: %1").arg(reply.object().value("error").toString()));
                    return;
                }
                _cleanup_plan = reply.object().toVariantMap();
                set_message(tr("Review the cleanup preview. Nothing has been deleted."));
                emit cleanup_plan_changed();
            });
}

void ExtensionClient::execute_cleanup()
{
    auto id = _cleanup_plan.value("id").toString();
    if (id.isEmpty() || _busy || !connected()) return;
    _cleanup_plan.clear();
    set_message(tr("Removing reviewed unused packages…"), true);
    request("POST", "/v1/cleanup/execute", QJsonDocument(QJsonObject{{"id", id}}).toJson(QJsonDocument::Compact),
            [this](int status, const QJsonDocument &reply) {
                set_message(status == 200 ? tr("Cleanup finished. Reclaimed %1 MiB.").arg(reply.object().value("freed_bytes").toDouble() / 1048576, 0, 'f', 1)
                                         : tr("Cleanup refused: %1. Refresh and review a new preview.").arg(reply.object().value("error").toString()));
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
            set_message(tr("The FrameLab service did not answer; nothing was changed."));
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
    set_message(tr("Stopping FrameLab"), true);
    with_fresh_state([this, camera] {
        auto revision = this->camera(camera).value("revision", 0).toInt();
        request("DELETE",
                QString("/v1/cameras/%1/extension?revision=%2")
                    .arg(QString(QUrl::toPercentEncoding(camera)), QString::number(revision)),
                {}, [this](int status, const QJsonDocument &reply) {
                    set_message(status == 200 ? tr("FrameLab stopped; raw preview restored.")
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
    if (!_wanted || !connected()) return;
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
    if (type == "job" || type == "package_added" || type == "package_deleted" || type == "cleanup") refresh_packages();
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
        // Keep raw decoding active for immediate fallback and the Raw / Analytics switch.
        // A fresh service image does not prove that a visible main presenter is ready.
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
