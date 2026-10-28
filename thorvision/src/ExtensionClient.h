#pragma once

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
#include <QWebSocket>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

class CameraModel;
class CameraItem;

// Connection to the device's analytics extension service.
//
// The service only listens on device loopback. This client reaches it through an OpenSSH
// tunnel run with BatchMode and strict host-key checking: it uses the user's SSH key/agent and
// known_hosts, never asks for or stores a password, and refuses unknown or changed host keys.
// It also owns the PC side of analytics recording: a bounded live result cache and the
// sidecars written next to PC recording segments (same format as the device recorder).
class ExtensionClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString connection_state READ connection_state NOTIFY connection_changed)
    Q_PROPERTY(QString connection_detail READ connection_detail NOTIFY connection_changed)
    Q_PROPERTY(bool connected READ connected NOTIFY connection_changed)
    Q_PROPERTY(QString device_host READ device_host CONSTANT)
    Q_PROPERTY(QString ssh_user READ ssh_user WRITE set_ssh_user NOTIFY settings_changed)
    Q_PROPERTY(int remote_port READ remote_port WRITE set_remote_port NOTIFY settings_changed)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY connection_changed)
    Q_PROPERTY(QVariantMap state READ state NOTIFY state_changed)
    Q_PROPERTY(QVariantList packages READ packages NOTIFY packages_changed)
    Q_PROPERTY(QString message READ message NOTIFY message_changed)
    Q_PROPERTY(bool busy READ busy NOTIFY message_changed)

public:
    explicit ExtensionClient(CameraModel &cameras, QObject *parent = nullptr);
    ~ExtensionClient() override;

    QString connection_state() const { return _connection_state; }
    QString connection_detail() const { return _connection_detail; }
    bool connected() const { return _connection_state == "Connected"; }
    // Local end of the SSH tunnel, for the agent bridge's ThorVision tools; empty when not connected.
    QString extensions_url() const { return connected() ? QString("http://127.0.0.1:%1").arg(_local_port) : QString(); }
    QString device_host() const;
    QString ssh_user() const { return _ssh_user; }
    void set_ssh_user(const QString &user);
    int remote_port() const { return _remote_port; }
    void set_remote_port(int port);
    QVariantMap capabilities() const { return _capabilities; }
    QVariantMap state() const { return _state; }
    QVariantList packages() const { return _packages; }
    QString message() const { return _message; }
    bool busy() const { return _busy; }

    Q_INVOKABLE void connect_device();
    Q_INVOKABLE void disconnect_device();
    Q_INVOKABLE QVariantMap camera(const QString &camera) const;
    Q_INVOKABLE QStringList camera_ids() const;
    Q_INVOKABLE void refresh_packages();
    Q_INVOKABLE void upload_package(const QUrl &archive);
    Q_INVOKABLE void test_package(const QString &digest);
    Q_INVOKABLE void apply(const QString &camera, const QString &digest);
    Q_INVOKABLE void stop(const QString &camera);
    Q_INVOKABLE void roll_back(const QString &camera);
    Q_INVOKABLE bool preview_fresh(const QString &camera) const;
    Q_INVOKABLE QVariantMap preview_header(const QString &camera) const;
    // Results held for PC recording sidecars (about the last 60 s).
    Q_INVOKABLE int cached_results(const QString &camera) const { return int(_results.value(camera).size()); }

    QImage preview(const QString &camera) const;
    void watch_preview(const QString &camera, bool watching);
    // Matches cached results to the frames actually in a closed PC recording segment.
    void write_pc_sidecar(const QString &camera, const QString &path);

signals:
    void connection_changed();
    void settings_changed();
    void state_changed();
    void packages_changed();
    void message_changed();
    void preview_updated(const QString &camera);
    void preview_freshness_changed(const QString &camera);
    void sidecar_written(const QString &path, bool written, bool complete);

private:
    struct Preview {
        QImage image;
        QVariantMap header;
        qint64 received_ms = 0;
        bool fresh = false;
        int watchers = 0;
        std::unique_ptr<QWebSocket> socket;
    };
    struct CachedRecord {
        qint64 received_ms;
        nlohmann::json record;
    };

    CameraModel &_cameras;
    QNetworkAccessManager _network;
    QProcess _tunnel;
    QTimer _poll, _freshness, _reconnect;
    QWebSocket _events;
    QString _connection_state = "Disconnected", _connection_detail, _ssh_user, _message;
    QString _tunnel_errors;
    int _remote_port = 8770, _local_port = 0, _failed_polls = 0, _capability_attempts = 0;
    bool _wanted = false, _busy = false;
    qint64 _events_cursor = 0;
    QVariantMap _capabilities, _state;
    QVariantList _packages;
    std::map<QString, Preview> _previews;
    QHash<QString, std::deque<CachedRecord>> _results;
    // First frame sequence per camera/epoch since the event stream became continuous.
    QHash<QString, std::map<std::string, std::uint64_t>> _covered_from;
    QHash<QObject *, bool> _hooked;

    QUrl url(const QString &path, const QString &scheme = "http") const;
    void set_connection(const QString &state, const QString &detail = {});
    void set_message(const QString &message, bool busy = false);
    void start_tunnel();
    void check_service();
    void poll_state();
    void open_events();
    void on_event(const QString &text);
    void open_preview(const QString &camera, Preview &preview);
    void on_preview(const QString &camera, const QByteArray &data);
    void update_freshness();
    void hook_cameras();
    void on_fragment_closed(const QString &camera, const QString &path);
    using Reply = std::function<void(int status, const QJsonDocument &body)>;
    void request(const QByteArray &method, const QString &path, const QByteArray &body, Reply done,
                 const QByteArray &idempotency_key = {});
    void watch_job(const QString &job, const QString &label);
    void with_fresh_state(std::function<void()> then);
    void teardown();
};
