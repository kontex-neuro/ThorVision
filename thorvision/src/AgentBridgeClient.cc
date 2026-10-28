#include "AgentBridgeClient.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <spdlog/spdlog.h>

#include "ExtensionClient.h"

namespace
{
constexpr int protocol_version = 1;

QString bridge_script()
{
    if (auto path = qEnvironmentVariable("THORVISION_AGENT_BRIDGE"); !path.isEmpty()) return path;
    auto beside = QDir(QCoreApplication::applicationDirPath()).filePath("agent-bridge/dist/main.js");
    if (QFileInfo::exists(beside)) return beside;
#ifdef THORVISION_AGENT_BRIDGE_DIR
    return QDir(THORVISION_AGENT_BRIDGE_DIR).filePath("dist/main.js");
#else
    return beside;
#endif
}

QString node_executable()
{
    if (auto path = qEnvironmentVariable("THORVISION_NODE"); !path.isEmpty()) return path;
    return QStandardPaths::findExecutable("node");
}
}  // namespace

AgentBridgeClient::AgentBridgeClient(ExtensionClient &extensions, QObject *parent)
    : QObject(parent), _extensions(extensions)
{
    QSettings settings;
    _agent = settings.value("agents/agent", "codex").toString();
    _ask = settings.value("agents/ask_before_tools", false).toBool();
    _process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&_process, &QProcess::readyReadStandardOutput, this, &AgentBridgeClient::receive);
    connect(&_process, &QProcess::readyReadStandardError, this, [this] {
        for (auto &line : QString::fromUtf8(_process.readAllStandardError()).split('\n', Qt::SkipEmptyParts))
            spdlog::debug("agent bridge: {}", line.toStdString());
    });
    connect(&_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        for (auto &done : _pending) done({}, QJsonObject{{"message", "agent bridge stopped"}});
        _pending.clear();
        _session.clear();
        _running = false;
        set_state("Stopped", tr("The agent bridge exited (%1).").arg(code));
    });
    connect(&_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) set_state("Unavailable", tr("Node.js could not start the agent bridge."));
    });
}

AgentBridgeClient::~AgentBridgeClient()
{
    if (_process.state() != QProcess::NotRunning) {
        // Closing stdin lets the bridge stop its provider processes; it never signs anyone out.
        _process.closeWriteChannel();
        if (!_process.waitForFinished(3000)) _process.kill();
    }
}

void AgentBridgeClient::set_state(const QString &state, const QString &detail)
{
    _state = state;
    _detail = detail;
    emit changed();
}

QString AgentBridgeClient::workspace() const
{
    if (_camera.isEmpty()) return {};
    // One extension project per camera, shared by every agent, with its validation history.
    auto digest = QCryptographicHash::hash(_camera.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("extensions/" + digest);
}

void AgentBridgeClient::set_agent(const QString &agent)
{
    if (agent == _agent) return;
    _agent = agent;
    QSettings().setValue("agents/agent", agent);
    new_conversation();
    refresh_status(agent);
}

void AgentBridgeClient::set_camera(const QString &camera)
{
    if (camera == _camera) return;
    _camera = camera;
    new_conversation();
}

void AgentBridgeClient::set_ask_before_tools(bool ask)
{
    if (ask == _ask) return;
    _ask = ask;
    QSettings().setValue("agents/ask_before_tools", ask);
    new_conversation();
}

void AgentBridgeClient::start()
{
    if (_process.state() != QProcess::NotRunning) return;
    auto node = node_executable();
    auto script = bridge_script();
    if (node.isEmpty() || !QFileInfo::exists(script)) {
        set_state("Unavailable", node.isEmpty() ? tr("Node.js was not found.") : tr("The agent bridge is not built: %1").arg(script));
        return;
    }
    set_state("Starting");
    _process.start(node, {script});
    request("bridge.initialize", {{"protocolVersion", protocol_version}, {"client", QJsonObject{{"name", "ThorVision"}}}},
            [this](const QJsonObject &result, const QJsonObject &error) {
                if (!error.isEmpty()) {
                    set_state("Unavailable", error.value("message").toString());
                    return;
                }
                set_state("Ready", tr("Bridge %1, SDK %2").arg(result.value("bridgeVersion").toString(),
                                                              result.value("sdk").toObject().value("sdk_version").toString()));
                request("agents.discover", {}, [this](const QJsonObject &result, const QJsonObject &) {
                    _agents = result.value("items").toArray().toVariantList();
                    emit changed();
                    refresh_status();
                });
            });
}

void AgentBridgeClient::request(const QString &method, const QJsonObject &params, Reply done)
{
    if (_process.state() != QProcess::Running) {
        if (done) done({}, QJsonObject{{"message", "agent bridge is not running"}});
        return;
    }
    auto id = _next_id++;
    if (done) _pending.insert(id, std::move(done));
    QJsonObject message{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
    _process.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void AgentBridgeClient::receive()
{
    _buffer += _process.readAllStandardOutput();
    for (qsizetype end; (end = _buffer.indexOf('\n')) >= 0;) {
        auto line = _buffer.left(end);
        _buffer.remove(0, end + 1);
        auto document = QJsonDocument::fromJson(line);
        if (!document.isObject()) continue;
        auto message = document.object();
        if (message.contains("method")) {
            on_notification(message.value("method").toString(), message.value("params").toObject());
            continue;
        }
        auto found = _pending.find(message.value("id").toInteger());
        if (found == _pending.end()) continue;
        auto done = std::move(found.value());
        _pending.erase(found);
        // Arrays (e.g. agents.discover) are wrapped so every reply is an object.
        auto result = message.value("result");
        done(result.isArray() ? QJsonObject{{"items", result}} : result.toObject(), message.value("error").toObject());
    }
}

void AgentBridgeClient::refresh_status(const QString &agent)
{
    for (auto &value : _agents) {
        auto id = value.toMap().value("id").toString();
        if (!agent.isEmpty() && id != agent) continue;
        request("agent.accountStatus", {{"agent", id}}, [this, id](const QJsonObject &result, const QJsonObject &error) {
            _statuses[id] = error.isEmpty() ? result.toVariantMap()
                                            : QVariantMap{{"state", "error"}, {"billing", "Unknown"}, {"detail", error.value("message").toString()}};
            emit changed();
        });
    }
}

void AgentBridgeClient::sign_in()
{
    request("agent.login", {{"agent", _agent}}, [this](const QJsonObject &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            append("error", error.value("message").toString());
            return;
        }
        // Provider-owned sign-in only: a browser page or the provider's own login command.
        if (auto url = result.value("url").toString(); !url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
        auto terminal = result.value("terminal").toObject();
        if (!terminal.isEmpty()) {
            QStringList args;
            for (auto arg : terminal.value("args").toArray()) args << arg.toString();
#ifdef _WIN32
            QProcess::startDetached("cmd", QStringList{"/c", "start", "cmd", "/k", terminal.value("command").toString()} + args);
#else
            QProcess::startDetached(terminal.value("command").toString(), args);
#endif
        }
        auto text = result.value("instructions").toString();
        if (auto code = result.value("code").toString(); !code.isEmpty()) text += tr(" Code: %1").arg(code);
        append("progress", text);
    });
}

void AgentBridgeClient::ensure_session(std::function<void()> then)
{
    auto key = _agent + '\n' + _camera + '\n' + (_ask ? "ask" : "auto");
    if (!_session.isEmpty() && key == _session_key) {
        then();
        return;
    }
    auto url = _extensions.extensions_url();
    if (url.isEmpty()) {
        append("error", tr("Connect to the device analytics service first."));
        return;
    }
    QDir().mkpath(workspace());
    request("session.create",
            {{"agent", _agent}, {"camera", _camera}, {"workspace", QDir::toNativeSeparators(workspace())}, {"extensionsUrl", url},
             {"approval", _ask ? "ask" : "auto"}},
            [this, key, then = std::move(then)](const QJsonObject &result, const QJsonObject &error) {
                if (!error.isEmpty()) {
                    _running = false;
                    append("error", tr("Could not start the %1 session: %2").arg(_agent, error.value("message").toString()));
                    emit changed();
                    return;
                }
                _session = result.value("sessionId").toString();
                _session_key = key;
                then();
            });
}

void AgentBridgeClient::send(const QString &text)
{
    if (text.trimmed().isEmpty() || _running) return;
    if (_camera.isEmpty()) {
        append("error", tr("Select a camera first."));
        return;
    }
    start();
    append("user", text);
    _running = true;
    emit changed();
    ensure_session([this, text] {
        request("session.prompt", {{"sessionId", _session}, {"text", text}},
                [this](const QJsonObject &, const QJsonObject &error) {
                    _running = false;
                    if (!error.isEmpty()) append("error", error.value("message").toString());
                    emit changed();
                    _extensions.refresh_packages();
                });
    });
}

void AgentBridgeClient::cancel()
{
    if (!_session.isEmpty()) request("session.cancel", {{"sessionId", _session}});
}

void AgentBridgeClient::new_conversation()
{
    if (!_session.isEmpty()) request("session.close", {{"sessionId", _session}});
    _session.clear();
    _session_key.clear();
    _running = false;
    _permission.clear();
    _transcript.clear();
    emit transcript_changed();
    emit changed();
}

void AgentBridgeClient::respond_permission(bool allow)
{
    if (_permission.isEmpty()) return;
    request("permission.respond", {{"requestId", _permission.value("requestId").toString()}, {"allow", allow}});
    append("progress", allow ? tr("Allowed %1").arg(_permission.value("tool").toString())
                             : tr("Denied %1").arg(_permission.value("tool").toString()));
    _permission.clear();
    emit changed();
}

void AgentBridgeClient::append(const QString &kind, const QString &text, const QVariantMap &extra)
{
    // Streamed agent text extends the previous agent entry.
    if (kind == "agent" && !_transcript.isEmpty()) {
        auto last = _transcript.last().toMap();
        if (last.value("kind") == "agent") {
            last["text"] = last.value("text").toString() + text;
            _transcript.last() = last;
            emit transcript_changed();
            return;
        }
    }
    auto entry = extra;
    entry["kind"] = kind;
    entry["text"] = text;
    _transcript.append(entry);
    emit transcript_changed();
}

void AgentBridgeClient::on_notification(const QString &method, const QJsonObject &params)
{
    if (method == "agent.status") {
        _statuses[params.value("agent").toString()] = params.toVariantMap();
        emit changed();
        return;
    }
    if (method != "session.event" || params.value("sessionId").toString() != _session) return;
    auto type = params.value("type").toString();
    if (type == "message") append("agent", params.value("text").toString());
    else if (type == "progress") append("progress", params.value("text").toString());
    else if (type == "toolStarted") append("tool", tr("Using %1").arg(params.value("tool").toString()));
    else if (type == "toolFinished") {
        append("tool", tr("%1 %2").arg(params.value("tool").toString(), params.value("ok").toBool() ? tr("finished") : tr("failed")));
        if (params.value("tool").toString().contains("test_candidate")) _extensions.refresh_packages();
    } else if (type == "permissionRequested") {
        _permission = params.toVariantMap();
        emit changed();
    } else if (type == "failed") {
        auto state = params.value("state").toString();
        append("error", params.value("message").toString(), {{"state", state}});
        // Account problems are connection states, not callback failures.
        if (state != "agent_error") refresh_status(params.value("agent").toString());
    }
}
