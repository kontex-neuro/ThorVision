#include "UpdateController.h"

#include <spdlog/spdlog.h>

#include <QMetaEnum>
#include <QPointer>
#include <chrono>
#include <filesystem>

#include "xdaqvc/camera.h"
#include "DeviceUpdateSelection.h"

using namespace xvc::update;

namespace
{
constexpr auto CLOUD_BASE_URL = "https://dl-thorvision-firmware.kontex.io";

// The device-server major.minor series this application speaks. Not derivable from libxvc's
// own version, which is an independent version line -- bump this deliberately as part of a
// release when the client starts requiring a new device-server series.
constexpr VersionSeries REQUIRED_SERIES{0, 1};



// A chatty device must not grow the log model without bound.
constexpr int MAX_LOG_LINES = 500;

// How long to wait for the device to come back after it applies an update and reboots.
constexpr auto RESTART_TIMEOUT = std::chrono::seconds{180};
constexpr auto RESTART_POLL_INTERVAL = std::chrono::seconds{2};

DeviceEndpoint device_endpoint() { return DeviceEndpoint{.host = "192.168.177.100"}; }

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
        return QObject::tr("Update connection failed: %1")
            .arg(QString::fromStdString(error.message));
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
        return QObject::tr("The device update could not be verified: %1").arg(QString::fromStdString(error.message));
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
    // A known incompatible API stays blocked after a failed or dismissed check.
    // Optional update failures release streams so ordinary operation can continue.
    if (_required && _state != State::UpToDate && _state != State::Succeeded) return true;
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
    if (in_progress() && !_phase_detail.isEmpty()) return _phase_detail;
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

    // A transfer may have restarted the server; refresh connections on return.
    // A failed health check alone does not establish that rollback succeeded.
    const auto device_restarted = _state == State::Uploading || _state == State::Restarting;

    _error_message = user_message_for(error);
    _recovery = _modern_plan && error.code == ErrorCode::DeviceUpdateFailed
        ? Recovery::Terminal : recovery_for(error.code);

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
    ++_check_generation;
    _released = false;
    _device_descriptor.reset();
    _plan.reset();
    _modern_plan.reset();
    _required = false;
    _device_did_restart = true;
    set_state(State::Idle);
}

void UpdateController::check_for_update()
{
    if (in_progress() || _state == State::Checking) return;
    const auto raw = qgetenv("THORVISION_DEVICE_UPDATE_CHANNEL");
    const auto channel = thorvision::updates::update_channel(
        qEnvironmentVariableIsSet("THORVISION_DEVICE_UPDATE_CHANNEL")
            ? std::optional<std::string_view>{std::string_view{raw.constData(), static_cast<std::size_t>(raw.size())}}
            : std::nullopt);
    if (!channel) {
        ++_check_generation;
        join_worker();
        _plan.reset();
        _modern_plan.reset();
        _error_message = QString::fromStdString(channel.error().message);
        _recovery = Recovery::Terminal;
        set_state(State::Failed);
        return;
    }
    begin_check(*channel);
}

void UpdateController::check_development_update() { begin_check("dev"); }

void UpdateController::begin_check(std::string environment)
{
    if (in_progress() || _state == State::Checking) return;

    const auto generation = ++_check_generation;
    join_worker();
    _environment = std::move(environment);
    _required = false;
    _device_descriptor.reset();
    _modern_plan.reset();
    _plan.reset();
    _dismissed = false;
    emit dismissed_changed();

    set_state(State::Checking);
    _worker = std::jthread([this,generation](std::stop_token) { run_check(generation); });
}

void UpdateController::run_check(std::uint64_t generation)
{
    auto dispatch = [this,generation](auto callback) {
        QMetaObject::invokeMethod(this,[this,generation,callback=std::move(callback)]() mutable {
            if (generation == _check_generation.load()) callback();
        },Qt::QueuedConnection);
    };
    auto identity = discover_device(device_endpoint());
    if (!identity) {
        const auto error = identity.error();
        dispatch([this,error]{fail(error);});
        return;
    }
    dispatch([this,value=*identity]{
        _device_descriptor=value;
        _required=!thorvision::updates::api_compatible(value, REQUIRED_SERIES);
        emit plan_changed();
    });
    auto matrix = fetch_versions_json(CLOUD_BASE_URL);
    if (!matrix) {
        const auto error = matrix.error();
        dispatch([this, error] { fail(error); });
        return;
    }

    const bool legacy_aio_opt_in = qEnvironmentVariable("THORVISION_ALLOW_LEGACY_AIO_MIGRATION") == "1";
    std::string updater_version;
    if (identity->modern || legacy_aio_opt_in) {
        auto updater = discover_updater_version(device_endpoint());
        if (!updater) {
            const auto error=updater.error();
            dispatch([this,error]{fail(error);});
            return;
        }
        updater_version = *updater;
    }
    auto selected = thorvision::updates::select(*matrix,*identity,updater_version,
                                                _environment,REQUIRED_SERIES,legacy_aio_opt_in);
    if (!selected) {
        const auto error=selected.error();
        dispatch([this,error]{fail(error);});
        return;
    }
    if (const auto legacy=std::get_if<UpdatePlan>(&*selected)) {
        dispatch([this,result=*legacy]{
            _modern_plan.reset();
            _plan=result;
            _device_version=QString::fromStdString(result.device_version.to_string());
            _target_version=QString::fromStdString(result.target.version.to_string());
            _changelog=QString::fromStdString(result.target.changelog);
            _required=result.required;
            emit plan_changed();
            set_state(result.action==UpdateAction::Update?State::UpdateAvailable:State::UpToDate);
        });
        return;
    }
    const auto plan=std::get_if<DeviceUpdatePlan>(&*selected);

    dispatch([this,result=*plan]{
        _modern_plan=result;
        _plan=UpdatePlan{result.update_available?UpdateAction::Update:UpdateAction::None,
                         result.device.build_version.value_or(Version{}),result.target,false,{}};
        _device_version=result.device.build_version ? QString::fromStdString(result.device.build_version->to_string()) : tr("Unknown application build");
        _target_version=QString::fromStdString(result.target.version.to_string());
        _changelog=QString::fromStdString(result.target.changelog);
        if(result.legacy_aio_confirmation)
            _changelog=tr("AIO only: this first upgrades the updater after checking the Jetson hardware and known legacy source. The application package follows only after a verified migration result.\n\n")+_changelog;
        if(result.environment=="dev")
            _changelog=tr("DEV test package. Installation also requires maintenance mode on the device.\n\n")+_changelog;
        _required=false;
        emit plan_changed();
        set_state(result.update_available?State::UpdateAvailable:State::UpToDate);
    });
}

void UpdateController::start_update()
{
    if (!_plan || _plan->action != UpdateAction::Update || in_progress() || _recording) return;

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
    if (_modern_plan) { run_modern_update(); return; }
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

        auto identity = discover_device(device);
        if (!identity || !identity->build_version) continue;
        const auto version = identity->build_version;

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

        const bool in_series = thorvision::updates::api_compatible(*identity, REQUIRED_SERIES)
            && *version == target.version;
        const auto now = QString::fromStdString(version->to_string());

        QMetaObject::invokeMethod(
            this,
            [this, now, in_series] {
                _device_version = now;
                if (in_series) _required = false;
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
    if (_required) return;
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
    if (_required) return;
    if (_state == State::Failed) {
        // A failure the user acknowledged should not immediately re-prompt on the next
        // reconnect; the badge remains as the standing reminder.
        _dismissed = true;
        emit dismissed_changed();
    }
    set_state(_state == State::Succeeded ? State::UpToDate : State::Dismissed);
}

void UpdateController::run_modern_update()
{
    const auto plan=*_modern_plan;
    auto progress=[this](std::uint64_t done,std::uint64_t total){
        const auto value=total?double(done)/double(total):0.0;
        QMetaObject::invokeMethod(this,[this,value,total]{set_progress(value,total==0);},Qt::QueuedConnection);
        return !_cancelled.load();
    };
    auto logs=[this](const LogEntry &entry){
        if(is_stream_end(entry)) return;
        const auto line=QString::fromStdString(entry.message);
        QMetaObject::invokeMethod(this,[this,line]{append_log(line);},Qt::QueuedConnection);
    };
    auto phase=[this](std::string_view message){
        const auto detail=QString::fromUtf8(message.data(),static_cast<qsizetype>(message.size()));
        QMetaObject::invokeMethod(this,[this,detail]{
            _phase_detail=detail;
            if(detail.startsWith("Downloading")) set_state(State::Downloading);
            else if(detail.startsWith("Connecting")) set_state(State::Handshaking);
            else if(detail.startsWith("Installing") || detail.startsWith("Verifying AIO")) set_state(State::Uploading);
            else set_state(State::Restarting);
            set_progress(0.0,true); emit state_changed();
        },Qt::QueuedConnection);
    };
    auto result=install_device_release(CLOUD_BASE_URL,device_endpoint(),plan,progress,logs,phase);
    if(!result){
        const auto error=result.error();
        QMetaObject::invokeMethod(this,[this,error]{fail(error);},Qt::QueuedConnection);
        return;
    }
    QMetaObject::invokeMethod(this,[this,plan]{
        _device_version=QString::fromStdString(plan.target.version.to_string());
        _required=false;
        _phase_detail.clear(); emit plan_changed(); arm_restart_after_update(); set_state(State::Succeeded);
    },Qt::QueuedConnection);
}
