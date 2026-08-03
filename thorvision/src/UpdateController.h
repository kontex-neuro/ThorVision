#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>

#include "xdaqvc/update/update.h"


// Drives the device-server update sequence and exposes it to QML as a single state machine.
//
// Kept separate from Server: Server is a connection monitor with one job, whereas an update
// owns a worker thread, a cancellation flag and a multi-step lifecycle. Every libxvc
// callback fires on a worker thread and is marshalled back here with a queued invocation --
// see the threading contract in xdaqvc/update/types.h.
class UpdateController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(State state READ state NOTIFY state_changed)
    Q_PROPERTY(QString device_version READ device_version NOTIFY plan_changed)
    Q_PROPERTY(QString target_version READ target_version NOTIFY plan_changed)
    Q_PROPERTY(QString changelog READ changelog NOTIFY plan_changed)
    Q_PROPERTY(bool required READ required NOTIFY plan_changed)
    Q_PROPERTY(double progress READ progress NOTIFY progress_changed)
    Q_PROPERTY(bool progress_indeterminate READ progress_indeterminate NOTIFY progress_changed)
    Q_PROPERTY(QString phase_text READ phase_text NOTIFY state_changed)
    Q_PROPERTY(int phase_index READ phase_index NOTIFY state_changed)
    Q_PROPERTY(int phase_count READ phase_count CONSTANT)
    Q_PROPERTY(bool cancellable READ cancellable NOTIFY state_changed)
    Q_PROPERTY(bool in_progress READ in_progress NOTIFY state_changed)
    Q_PROPERTY(QStringList logs READ logs NOTIFY logs_changed)
    Q_PROPERTY(QString error_message READ error_message NOTIFY state_changed)
    Q_PROPERTY(Recovery recovery READ recovery NOTIFY state_changed)
    Q_PROPERTY(bool update_dismissed READ update_dismissed NOTIFY dismissed_changed)
    Q_PROPERTY(bool streams_blocked READ streams_blocked NOTIFY state_changed)
    Q_PROPERTY(bool blocked_by_recording READ blocked_by_recording NOTIFY state_changed)

public:
    enum class State {
        Idle,
        Checking,
        UpToDate,
        UpdateAvailable,
        Downloading,
        Handshaking,
        Uploading,
        Restarting,
        Succeeded,
        Failed,
        Dismissed,
    };
    Q_ENUM(State)

    // How the failure dialog should offer to recover. Derived from ErrorCode, never from
    // the error message -- its wording is explicitly not part of the libxvc API.
    enum class Recovery {
        None,       // nothing went wrong
        Retryable,  // transport blip; Retry + Ignore
        Terminal,   // not user-fixable and not safe to auto-retry; Close + support
        Rollback,   // device restored itself; Retry + Quit
    };
    Q_ENUM(Recovery)

    explicit UpdateController(QObject *parent = nullptr);
    ~UpdateController() override;

    State state() const noexcept { return _state; }
    QString device_version() const { return _device_version; }
    QString target_version() const { return _target_version; }
    QString changelog() const { return _changelog; }
    bool required() const noexcept { return _required; }
    double progress() const noexcept { return _progress; }
    bool progress_indeterminate() const noexcept { return _progress_indeterminate; }
    QStringList logs() const { return _logs; }
    QString error_message() const { return _error_message; }
    Recovery recovery() const noexcept { return _recovery; }
    bool update_dismissed() const noexcept { return _dismissed; }

    QString phase_text() const;
    int phase_index() const noexcept;
    // Three, not four: upload / apply / reboot happen inside one blocking libxvc call and
    // cannot be reported as separate steps.
    static constexpr int phase_count() noexcept { return 3; }

    // Cancellable up to the point bytes start landing on the device. Downloading and the
    // handshake/prepare steps touch nothing on the device that a rollback cannot undo;
    // aborting mid-Upload risks a half-written image, so it stops there.
    bool cancellable() const noexcept
    {
        return _state == State::Downloading || _state == State::Handshaking;
    }
    bool in_progress() const noexcept;

    // Called when the device connects. Starts a check unless one was already dismissed this
    // session, so a flapping connection cannot re-prompt repeatedly.
    void on_device_connected();
    void on_device_disconnected();

    // True while the update question is unresolved and streams must stay stopped.
    //
    // A successful update restarts the device server. Any pipeline started beforehand would
    // still be pointed at the old server and end up in an undefined state, so nothing may
    // stream until we know whether an update is happening.
    bool streams_blocked() const noexcept;

    // True only while the device itself is being torn down and restarted by an update.
    //
    // Distinct from streams_blocked(): that stays true before the question is answered and
    // whenever the device is simply absent, which are NOT reasons to discard a genuine
    // hotplug event. Conflating the two silently drops every camera add/remove once the
    // state returns to Idle.
    bool device_restarting() const noexcept { return in_progress(); }

    // The update prompt is not startup-only: Server polls and re-emits status_change on
    // every reconnect, so an offer can arrive mid-session while a recording is running.
    // Killing a recording to install an update would be worse than making the update wait.
    bool blocked_by_recording() const noexcept { return _recording; }
    void set_recording(bool recording);

public slots:
    void check_for_update();  // manual entry point, from the menu; clears `dismissed`
    void start_update();
    void cancel();
    void ignore();
    void dismiss_result();  // acknowledge Succeeded / Failed and return to a resting state

    // True when the release that is being handled follows a device restart, so the caller
    // knows connections bound to the old server must be re-established.
    bool device_did_restart() const noexcept { return _device_did_restart; }

signals:
    void state_changed();
    void plan_changed();
    void progress_changed();
    void logs_changed();
    void dismissed_changed();

    // Emitted once the update question resolves in any way -- up to date, ignored, failed
    // check, or a completed update -- and streaming may proceed. A failed check resolves to
    // "start streams": an unreachable update service must never brick a local session.
    void streams_released();

private:
    void set_state(State state);
    void set_progress(double value, bool indeterminate);
    void append_log(const QString &line);
    void fail(const xvc::update::Error &error);
    void join_worker();
    void release_streams();  // idempotent; fires streams_released() at most once
    void arm_restart_after_update();  // call BEFORE the state transition, not after

    // Worker-thread only. Polls the device's camera list until it stops changing, so the
    // rebuild does not race the device's own start-up.
    bool wait_for_stable_camera_list(std::chrono::steady_clock::time_point deadline);

    // Runs on _worker. Everything it touches on `this` is marshalled back to the GUI thread.
    void run_check();
    void run_update();

    State _state = State::Idle;
    QString _device_version;
    QString _target_version;
    QString _changelog;
    bool _required = false;
    double _progress = 0.0;
    bool _progress_indeterminate = true;
    QStringList _logs;
    QString _error_message;
    Recovery _recovery = Recovery::None;
    bool _dismissed = false;
    bool _recording = false;
    bool _released = false;
    bool _device_did_restart = false;

    // Written by the GUI thread, read by the worker -- must be atomic.
    std::atomic<bool> _cancelled{false};

    std::optional<xvc::update::UpdatePlan> _plan;
    std::jthread _worker;
};
