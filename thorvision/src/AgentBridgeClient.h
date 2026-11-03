#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class ExtensionClient;

// Launches the local TypeScript agent bridge (agent-bridge/dist/main.js) and speaks its
// versioned JSON-RPC protocol over stdio. Provider sign-in stays with each provider's own
// runtime; this class never sees or stores AI credentials.
class AgentBridgeClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantList agents READ agents NOTIFY changed)
    Q_PROPERTY(QVariantMap statuses READ statuses NOTIFY changed)
    Q_PROPERTY(QString agent READ agent WRITE set_agent NOTIFY changed)
    Q_PROPERTY(QString camera READ camera WRITE set_camera NOTIFY changed)
    Q_PROPERTY(bool ask_before_tools READ ask_before_tools WRITE set_ask_before_tools NOTIFY changed)
    Q_PROPERTY(QVariantList transcript READ transcript NOTIFY transcript_changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QVariantMap pending_permission READ pending_permission NOTIFY changed)
    Q_PROPERTY(QString workspace READ workspace NOTIFY changed)

public:
    AgentBridgeClient(ExtensionClient &extensions, QObject *parent = nullptr);
    ~AgentBridgeClient() override;

    QString state() const { return _state; }
    QString detail() const { return _detail; }
    QVariantList agents() const { return _agents; }
    QVariantMap statuses() const { return _statuses; }
    QString agent() const { return _agent; }
    void set_agent(const QString &agent);
    QString camera() const { return _camera; }
    void set_camera(const QString &camera);
    bool ask_before_tools() const { return _ask; }
    void set_ask_before_tools(bool ask);
    QVariantList transcript() const { return _transcript; }
    bool running() const { return _running || _preparing; }
    QVariantMap pending_permission() const { return _permission; }
    QString workspace() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void refresh_status(const QString &agent = {});
    Q_INVOKABLE void sign_in();
    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void new_conversation();
    Q_INVOKABLE void respond_permission(bool allow);
    Q_INVOKABLE void prepare_package(const QString &digest);

signals:
    void changed();
    void transcript_changed();

private:
    using Reply = std::function<void(const QJsonObject &result, const QJsonObject &error)>;

    ExtensionClient &_extensions;
    QProcess _process;
    QByteArray _buffer;
    qint64 _next_id = 1;
    QHash<qint64, Reply> _pending;
    QString _state = "Stopped", _detail, _agent = "codex", _camera, _session, _session_key;
    QVariantList _agents, _transcript;
    QVariantMap _statuses, _permission;
    bool _ask = false, _running = false;
    bool _preparing = false;
    QString _pending_package;

    void request(const QString &method, const QJsonObject &params, Reply done = {});
    void receive();
    void on_notification(const QString &method, const QJsonObject &params);
    void append(const QString &kind, const QString &text, const QVariantMap &extra = {});
    void ensure_session(std::function<void()> then);
    void set_state(const QString &state, const QString &detail = {});
};
