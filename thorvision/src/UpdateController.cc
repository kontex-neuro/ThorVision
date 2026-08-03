#include "UpdateController.h"

#include <spdlog/spdlog.h>

#include <QMetaEnum>
#include <QPointer>
#include <chrono>
#include <filesystem>

#include "xdaqvc/camera.h"

using namespace xvc::update;

namespace
{
// TODO: replace with the production update endpoint once it is assigned.
constexpr auto CLOUD_BASE_URL = "dl-thorvision-firmware.kontex.io";

// The device-server major.minor series this application speaks. Not derivable from libxvc's
// own version, which is an independent version line -- bump this deliberately as part of a
// release when the client starts requiring a new device-server series.
constexpr VersionSeries REQUIRED_SERIES{0, 1};

constexpr auto DEVICE_HOST = "192.168.177.100";

// A chatty device must not grow the log model without bound.
constexpr int MAX_LOG_LINES = 500;

// How long to wait for the device to come back after it applies an update and reboots.
constexpr auto RESTART_TIMEOUT = std::chrono::seconds{180};
constexpr auto RESTART_POLL_INTERVAL = std::chrono::seconds{2};

DeviceEndpoint device_endpoint() { return DeviceEndpoint{.host = DEVICE_HOST}; }

UpdateController::Recovery recovery_for(ErrorCode code)
{
    switch (code) {
    case ErrorCode::NetworkUnreachable:
    case ErrorCode::HttpError:
    case ErrorCode::SizeMismatch:
    case ErrorCode::HandshakeRejected:
        return UpdateController::Recovery::Retryable;

    case ErrorCode::DeviceUpdateFailed:
        return UpdateController::Recovery::Rollback;

    // HashMismatch is deliberately NOT retryable: on a correctly-sized file it indicates
    // corruption or tampering rather than a transport blip.
    case ErrorCode::HashMismatch:
    case ErrorCode::MalformedResponse:
    case ErrorCode::PlatformNotFound:
    case ErrorCode::VersionNotFound:
    case ErrorCode::ReleaseDeprecated:
    case ErrorCode::DowngradeRejected:
    case ErrorCode::FilesystemError:
    case ErrorCode::TransferRejected:
    case ErrorCode::Cancelled:
        return UpdateController::Recovery::Terminal;
    }
    return UpdateController::Recovery::Terminal;
}

QString user_message_for(const Error &error)
{
    switch (error.code) {
    case ErrorCode::NetworkUnreachable:
        return QObject::tr("Could not reach the update service. Check your internet connection."
        );
    case ErrorCode::HttpError:
        return QObject::tr("The update service returned an error (HTTP %1).")
            .arg(error.http_status);
    case ErrorCode::MalformedResponse:
        return QObject::tr("The update service returned unreadable data.");
    case ErrorCode::PlatformNotFound:
        return QObject::tr("No update is published for this device type. Please contact support."
        );
    case ErrorCode::VersionNotFound:
        return QObject::tr("No compatible update is published for this device. Please contact "
                           "support.");
    case ErrorCode::ReleaseDeprecated:
        return QObject::tr("That release has been withdrawn and can no longer be installed.");
    case ErrorCode::DowngradeRejected:
        return QObject::tr("The selected version is older than the version on the device.");
    case ErrorCode::SizeMismatch:
        return QObject::tr("The download was incomplete. Please try again.");
    case ErrorCode::HashMismatch:
        return QObject::tr("The downloaded update failed its integrity check and was discarded. "
                           "Try again later; if this keeps happening, contact support.");
    case ErrorCode::FilesystemError:
        return QObject::tr("Could not write the update file: %1")
            .arg(QString::fromStdString(error.message));
    case ErrorCode::Cancelled:
        return QObject::tr("The update was cancelled.");
    case ErrorCode::HandshakeRejected:
        return QObject::tr("The device refused the update request. It may be busy -- try again.");
    case ErrorCode::TransferRejected:
        return QObject::tr("The device refused the update package. It may not have enough free "
                           "space.");
    case ErrorCode::DeviceUpdateFailed:
        return QObject::tr("The device could not apply the update and has restored its previous "
                           "version.");
    }
    return QObject::tr("The update failed.");
}
}  // namespace


UpdateController::UpdateController(QObject *parent) : QObject(parent) {}

UpdateController::~UpdateController()
{
    _cancelled.store(true);
    join_worker();
}

void UpdateController::join_worker()
{
    if (_worker.joinable()) {
        _worker.request_stop();
        _worker.join();
    }
}

bool UpdateController::streams_blocked() const noexcept
{
    // Blocked only while the answer is genuinely unknown or an update is running. Every
    // other state -- including a failed check -- is a resolved question.
    switch (_state) {
    case State::Idle:
    case State::Checking:
    case State::UpdateAvailable:
        return true;
    default:
        return in_progress();
    }
}

void UpdateController::release_streams()
{
    if (_released) return;
    _released = true;
    spdlog::info("Update question resolved; starting camera streams");
    emit streams_released();
}

void UpdateController::set_recording(bool recording)
{
    if (_recording == recording) return;
    _recording = recording;
    emit state_changed();
}

bool UpdateController::in_progress() const noexcept
{
    switch (_state) {
    case State::Downloading:
    case State::Handshaking:
    case State::Uploading:
    case State::Restarting:
        return true;
    default:
        return false;
    }
}

int UpdateController::phase_index() const noexcept
{
    switch (_state) {
    case State::Downloading:
        return 1;
    case State::Handshaking:
        return 2;
    case State::Uploading:
    case State::Restarting:
        // Deliberately share step 3. transfer_file_and_apply_update() uploads, applies and
        // reboots the device inside ONE blocking call, so the client cannot observe the
        // boundary between them. Numbering them separately promised a transition we cannot
        // actually report: step 3 ran for ~47s while step 4 lasted under a second.
        return 3;
    default:
        return 0;
    }
}

QString UpdateController::phase_text() const
{
    switch (_state) {
    case State::Checking:
        return tr("Checking for updates");
    case State::Downloading:
        return tr("Downloading update");
    case State::Handshaking:
        return tr("Preparing device");
    case State::Uploading:
        // Covers the whole opaque call: the upload, the device applying it, and the reboot.
        // Naming only the upload left the user watching a bar that had long since stopped
        // being about transfer.
        return tr("Installing on device and restarting");
    case State::Restarting:
        return tr("Waiting for device to come back");
    default:
        return {};
    }
}

void UpdateController::set_state(State state)
{
    if (_state == state) return;
    _state = state;
    // Logged on the GUI thread, so the timestamp is when the UI could actually repaint --
    // not when the worker requested the change. The gap between the two is the thing to
    // look at when a phase appears to be skipped on screen.
    spdlog::info(
        "Update state -> {} (step {}/{})",
        QMetaEnum::fromType<UpdateController::State>().valueToKey(static_cast<int>(state)),
        phase_index(),
        phase_count()
    );
    emit state_changed();

    // Routed through the single state choke point so no resolution path can forget to
    // release streams and leave the app permanently blank.
    if (!streams_blocked()) release_streams();
}

void UpdateController::set_progress(double value, bool indeterminate)
{
    _progress = value;
    _progress_indeterminate = indeterminate;
    emit progress_changed();
}

void UpdateController::append_log(const QString &line)
{
    _logs.append(line);
    while (_logs.size() > MAX_LOG_LINES) {
        _logs.removeFirst();
    }
    emit logs_changed();
}

void UpdateController::fail(const Error &error)
{
    // Cancellation is not an error the user needs to be told about -- they just asked for it.
    if (error.code == ErrorCode::Cancelled) {
        spdlog::info("Device update cancelled by user");
        set_state(State::Dismissed);
        return;
    }

    // A platform miss is a deployment bug, not a benign "nothing to do" -- log it loudly so
    // it cannot hide in the field as an up-to-date device.
    if (error.code == ErrorCode::PlatformNotFound || error.code == ErrorCode::VersionNotFound) {
        spdlog::critical("Device update deployment error: {}", error.message);
    } else {
        spdlog::error("Device update failed: {}", error.message);
    }

    // A failure at or past the transfer means the device rebooted -- the firmware rolls back
    // and restarts the server -- so whatever the model held belongs to a server that is gone.
    // Rebuild it from the restored one rather than releasing against stale entries.
    const auto device_restarted = _state == State::Uploading || _state == State::Restarting;

    _error_message = user_message_for(error);
    _recovery = recovery_for(error.code);

    if (device_restarted) arm_restart_after_update();
    set_state(State::Failed);
}

void UpdateController::on_device_connected()
{
    if (in_progress()) return;

    // Already answered this session, or answering it now would interrupt a recording. Either
    // way the question is closed -- release streams so the app stays usable.
    if (_dismissed || _recording) {
        if (_recording) {
            spdlog::info("Skipping update check: a recording is in progress");
        }
        set_state(State::Dismissed);
        release_streams();
        return;
    }
    check_for_update();
}

void UpdateController::on_device_disconnected()
{
    // A disconnect during Restarting is expected -- the device is rebooting into the new
    // version and run_update() is already waiting for it. Anything else returns to rest.
    if (_state == State::Restarting || in_progress()) return;
    if (_state == State::Checking) set_state(State::Idle);
}

void UpdateController::check_for_update()
{
    if (in_progress() || _state == State::Checking) return;

    _dismissed = false;
    emit dismissed_changed();

    join_worker();
    set_state(State::Checking);
    _worker = std::jthread([this](std::stop_token) { run_check(); });
}

void UpdateController::run_check()
{
    auto matrix = fetch_versions_json(CLOUD_BASE_URL);
    if (!matrix) {
        const auto error = matrix.error();
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    auto device_version = get_device_version(device_endpoint());
    if (!device_version) {
        const auto error = device_version.error();
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    auto plan = plan_update(*device_version, *matrix, REQUIRED_SERIES);
    if (!plan) {
        const auto error = plan.error();
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this, result = *plan] {
            _plan = result;
            _device_version = QString::fromStdString(result.device_version.to_string());
            _required = result.required;

            if (result.action == UpdateAction::None) {
                _target_version.clear();
                _changelog.clear();
                emit plan_changed();
                spdlog::info("Device server {} is compatible", _device_version.toStdString());
                set_state(State::UpToDate);
                return;
            }

            _target_version = QString::fromStdString(result.target.version.to_string());
            _changelog = QString::fromStdString(result.target.changelog);
            emit plan_changed();
            spdlog::info(
                "Device update available: {} -> {} ({})", _device_version.toStdString(),
                _target_version.toStdString(), result.required ? "required" : "optional"
            );
            set_state(State::UpdateAvailable);
        },
        Qt::QueuedConnection
    );
}

void UpdateController::start_update()
{
    if (!_plan || _plan->action != UpdateAction::None) {
        if (!_plan) return;
    }
    if (in_progress()) return;

    _cancelled.store(false);
    _logs.clear();
    emit logs_changed();
    _error_message.clear();
    _recovery = Recovery::None;

    join_worker();
    set_state(State::Downloading);
    set_progress(0.0, true);
    _worker = std::jthread([this](std::stop_token) { run_update(); });
}

void UpdateController::run_update()
{
    const auto device = device_endpoint();
    const auto target = _plan->target;

    auto marshal_progress = [this](std::uint64_t done, std::uint64_t total) {
        // WORKER THREAD. `total` is 0 when the server sends no Content-Length -- never
        // divide by it; the bar goes indeterminate instead.
        const auto indeterminate = total == 0;
        const auto value = indeterminate ? 0.0 : double(done) / double(total);
        QMetaObject::invokeMethod(
            this, [this, value, indeterminate] { set_progress(value, indeterminate); },
            Qt::QueuedConnection
        );
        return !_cancelled.load();
    };

    auto artifact = download_artifact(CLOUD_BASE_URL, target, marshal_progress);
    if (!artifact) {
        const auto error = artifact.error();
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this] {
            set_state(State::Handshaking);
            set_progress(0.0, true);
        },
        Qt::QueuedConnection
    );

    auto handshake = perform_handshake(device);
    if (!handshake) {
        const auto error = handshake.error();
        std::error_code ec;
        std::filesystem::remove(artifact->path, ec);
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    // perform_handshake/prepare_transfer take no progress callback, so the cancel flag has
    // no channel to interrupt them. Check between steps instead -- otherwise a cancel during
    // Handshaking sits unhonoured until the upload starts, which is exactly when we stop
    // allowing it.
    if (_cancelled.load()) {
        std::error_code ec;
        std::filesystem::remove(artifact->path, ec);
        QMetaObject::invokeMethod(
            this, [this] { fail({ErrorCode::Cancelled, "cancelled before transfer"}); },
            Qt::QueuedConnection
        );
        return;
    }

    auto prepared = prepare_transfer(device, *handshake, *artifact);
    if (!prepared) {
        const auto error = prepared.error();
        std::error_code ec;
        std::filesystem::remove(artifact->path, ec);
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    // Started before the transfer so no device-side log line is missed. The handle stops and
    // joins on destruction, so a failed update cannot leave this thread running.
    auto logs = stream_update_logs(device, *handshake, *prepared, [this](const LogEntry &entry) {
        if (is_stream_end(entry)) return;
        const auto line = QString::fromStdString(entry.message);
        QMetaObject::invokeMethod(
            this, [this, line] { append_log(line); }, Qt::QueuedConnection
        );
    });

    // Last point a cancel can be honoured: past here bytes are landing on the device.
    if (_cancelled.load()) {
        std::error_code ec;
        std::filesystem::remove(artifact->path, ec);
        QMetaObject::invokeMethod(
            this, [this] { fail({ErrorCode::Cancelled, "cancelled before transfer"}); },
            Qt::QueuedConnection
        );
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this] {
            set_state(State::Uploading);
            set_progress(0.0, true);
        },
        Qt::QueuedConnection
    );

    auto result =
        transfer_file_and_apply_update(device, *handshake, *prepared, artifact->path, marshal_progress);

    // Artifacts are transient -- libxvc keeps no cache, so this copy is ours to clean up.
    std::error_code ec;
    std::filesystem::remove(artifact->path, ec);

    if (!result) {
        const auto error = result.error();
        QMetaObject::invokeMethod(
            this, [this, error] { fail(error); }, Qt::QueuedConnection
        );
        return;
    }

    // Finished state comes from the return value above, never from the last progress tick --
    // queued delivery means a tick can still arrive after the call returns.
    QMetaObject::invokeMethod(
        this,
        [this] {
            set_state(State::Restarting);
            set_progress(0.0, true);
        },
        Qt::QueuedConnection
    );

    // The device reboots into the new version. Poll until it answers with a version inside
    // the required series, or give up with actionable advice rather than spinning forever.
    const auto deadline = std::chrono::steady_clock::now() + RESTART_TIMEOUT;
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(RESTART_POLL_INTERVAL);

        auto version = get_device_version(device);
        if (!version) continue;

        // /api_version answers as soon as the device's HTTP server is up, which is earlier
        // than its camera subsystem is ready. Enumerating at that moment returns stale
        // entries -- observed as a camera on a port that is replaced seconds later, leaving
        // an orphaned pipeline retrying SRT forever. Wait for the list to be non-empty and
        // report the same size twice in a row before declaring the device back.
        //
        // In practice this returns almost immediately: transfer_file_and_apply_update()
        // already blocked until the device was back, so by the time we get here the camera
        // list has usually settled. It stays as a guard for the case where it has not.
        if (!wait_for_stable_camera_list(deadline)) continue;

        const auto in_series =
            version->major() == REQUIRED_SERIES.major && version->minor() == REQUIRED_SERIES.minor;
        const auto now = QString::fromStdString(version->to_string());

        QMetaObject::invokeMethod(
            this,
            [this, now, in_series] {
                _device_version = now;
                emit plan_changed();
                if (in_series) {
                    spdlog::info("Device updated to {}", now.toStdString());
                    arm_restart_after_update();
                    set_state(State::Succeeded);
                } else {
                    _error_message =
                        tr("The device restarted but is still running version %1. The update did "
                           "not apply.")
                            .arg(now);
                    _recovery = Recovery::Rollback;
                    set_state(State::Failed);
                }
            },
            Qt::QueuedConnection
        );
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this] {
            _error_message = tr("The device did not come back online after the update. It may "
                                "still be starting up -- wait a moment, then power-cycle the "
                                "device and restart this application.");
            _recovery = Recovery::Terminal;
            set_state(State::Failed);
        },
        Qt::QueuedConnection
    );
}

void UpdateController::cancel()
{
    // Only meaningful while downloading. Once bytes are landing on the device, stopping
    // midway risks a half-written image, so the UI disables this and we ignore it here too.
    if (!cancellable()) return;
    spdlog::info("Cancelling device update");
    _cancelled.store(true);
}

void UpdateController::ignore()
{
    _dismissed = true;
    emit dismissed_changed();
    set_state(State::Dismissed);
}

bool UpdateController::wait_for_stable_camera_list(
    std::chrono::steady_clock::time_point deadline
)
{
    // Runs on the update worker thread; Camera::cameras() is a blocking HTTP call and must
    // never be made from the GUI thread.
    constexpr auto poll = std::chrono::milliseconds{750};
    std::optional<std::size_t> previous;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto count = Camera::cameras().size();

        // Two consecutive identical non-zero readings: the device has finished publishing
        // its cameras rather than being caught mid-enumeration.
        if (count > 0 && previous == count) {
            spdlog::info("Device camera list settled at {} camera(s)", count);
            return true;
        }
        previous = count;
        std::this_thread::sleep_for(poll);
    }

    spdlog::warn("Device camera list did not settle before the restart deadline");
    return false;
}

void UpdateController::arm_restart_after_update()
{
    // The device server restarted on a new version, so whatever the previous release built
    // is bound to a server that no longer exists. Clearing the latch lets the NEXT state
    // transition release again and rebuild against the new server.
    //
    // Must be called BEFORE the transition into Succeeded/Failed: set_state() releases as
    // soon as the state stops being blocked, so re-arming afterwards fires a second time and
    // builds the camera list twice -- once against the still-restarting device.
    _released = false;

    // Tells the release handler that connections bound to the old server -- the camera event
    // WebSocket in particular -- have to be re-established, not just re-read.
    _device_did_restart = true;
}

void UpdateController::dismiss_result()
{
    if (_state == State::Failed) {
        // A failure the user acknowledged should not immediately re-prompt on the next
        // reconnect; the badge remains as the standing reminder.
        _dismissed = true;
        emit dismissed_changed();
    }
    set_state(_state == State::Succeeded ? State::UpToDate : State::Dismissed);
}
