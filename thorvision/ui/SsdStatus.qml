pragma Singleton

import QtQuick

// Reads DeviceRecording.state for the SSD views. The device reports states as free text,
// so every view goes through these mappings to get the same icon, colour and wording.
//
// Status kinds: ok, safe, caution, critical, busy, off, unknown.
QtObject {
    id: root

    readonly property var snapshot: DeviceRecording.state
    readonly property var storage: snapshot.storage || ({})
    readonly property var cameras: snapshot.cameras || []
    readonly property bool connected: DeviceRecording.connected

    // First status only arrives after the first poll; until then nothing is known.
    readonly property bool has_status: snapshot.storage !== undefined

    readonly property string storage_state: connected ? (storage.state || qsTr("Unknown")) : qsTr("Unknown")
    readonly property string storage_kind: !connected ? "unknown" : kind_for_storage(storage.state)
    readonly property bool safe_to_remove: storage.state === "Safe to remove"
    readonly property bool has_volume: !!storage.uuid

    readonly property int selected_count: cameras.filter(c => c.enabled).length
    readonly property int recording_count: cameras.filter(c => c.enabled && is_recording(c.state)).length

    readonly property string summary: {
        if (!connected)
            return qsTr("Device may still be recording");
        if (!has_status)
            return "";
        if (selected_count === 0)
            return qsTr("No cameras selected for SSD recording");
        return qsTr("%1 of %2 selected cameras recording").arg(recording_count).arg(selected_count);
    }

    readonly property real retention_hours: Number(snapshot.estimated_history_seconds || 0) / 3600
    readonly property string retention: retention_hours > 0 ? qsTr("≈ %1 h").arg(retention_hours >= 10 ? Math.round(retention_hours) : retention_hours.toFixed(1)) : qsTr("Estimating…")

    // Device-wide history, oldest first, at most 200 entries, kept on the device across
    // reboots (videoserver recording/manager.py). Not limited to the current SSD.
    readonly property var events: snapshot.events || []

    // Combined "repair SSD and recover videos" job (videoserver maintenance API). The device
    // keeps the latest job, finished or not; null when there has never been one.
    readonly property var maintenance: snapshot.maintenance || null
    readonly property bool maintenance_active: !!maintenance && (maintenance.status === "queued" || maintenance.status === "running")
    readonly property bool maintenance_result_pending: !!maintenance && !maintenance_active && maintenance.id !== DeviceRecording.dismissed_job
    readonly property bool repair_enabled: !!storage.repair_enabled

    // Ordered phases a running job moves through. Some can finish between polls, so the UI
    // only marks earlier phases done and never treats a skipped one as an error.
    readonly property var maintenance_phases: ["stopping_recording", "backing_up", "recovering_videos", "repairing_filesystem", "restoring_videos"]

    function phase_text(phase) {
        switch (phase) {
        case "queued":
            return qsTr("Waiting to start");
        case "stopping_recording":
            return qsTr("Finishing current SSD recording files");
        case "backing_up":
            return qsTr("Backing up interrupted recordings");
        case "recovering_videos":
            return qsTr("Recovering and validating videos");
        case "repairing_filesystem":
            return qsTr("Repairing and checking the SSD filesystem");
        case "restoring_videos":
            return qsTr("Restoring recovered videos to the SSD");
        case "complete":
            return qsTr("Operation complete");
        case "failed":
            return qsTr("Operation failed");
        case "interrupted":
            return qsTr("Operation interrupted");
        }
        return phase || "";
    }

    // Counts for the running phase, e.g. "1 of 3 backed up". Empty when there is nothing
    // to count yet; never a percentage, which the API doesn't provide.
    function maintenance_counts(job) {
        if (!job)
            return "";
        const total = job.files_total || 0;
        if (job.phase === "backing_up")
            return total ? qsTr("%1 of %2 backed up").arg(job.files_backed_up || 0).arg(total) : "";
        if (job.phase === "recovering_videos")
            return total ? qsTr("%1 of %2 processed").arg(job.files_processed || 0).arg(total) : "";
        if (total === 0 && job.phase !== "queued" && job.phase !== "stopping_recording")
            return qsTr("no interrupted recordings found");
        return "";
    }

    // Result of a finished job as { kind, title, text }. "completed" means the filesystem is
    // ready, not that cameras are writing again; the camera list says that.
    function maintenance_result(job) {
        if (!job)
            return null;
        const recovered = job.files_recovered || 0;
        const failed = job.files_failed || 0;
        switch (job.status) {
        case "completed":
            return {
                kind: "ok",
                title: qsTr("SSD repaired"),
                text: !job.files_total ? qsTr("No interrupted recordings needed recovery. Recording resumes in new files once the SSD is ready.") : recovered === 1 ? qsTr("1 video recovered. Recording resumes in new files once the SSD is ready.") : qsTr("%1 videos recovered. Recording resumes in new files once the SSD is ready.").arg(recovered)
            };
        case "completed_with_warnings":
            return {
                kind: "caution",
                title: qsTr("SSD repaired with warnings"),
                text: qsTr("%1 of %2 videos couldn't be recovered. Their originals are backed up on the device.").arg(failed).arg(recovered + failed)
            };
        case "failed":
            return {
                kind: "critical",
                title: qsTr("SSD repair failed"),
                text: job.reason || qsTr("Check the SSD status before trying again.")
            };
        case "interrupted":
            return {
                kind: "caution",
                title: qsTr("SSD repair interrupted"),
                text: qsTr("The device restarted during the repair. Check the SSD status before trying again.")
            };
        }
        return null;
    }

    // The one thing that needs attention now, or null. Priority: connection, operation
    // error, SSD, then the first selected camera that is not recording.
    readonly property var current_issue: {
        if (!connected)
            return {
                kind: "unknown",
                text: qsTr("Connection to the device lost. Reconnecting… This doesn't stop recording on the device.")
            };
        if (maintenance_active) {
            const counts = maintenance_counts(maintenance);
            return {
                kind: "busy",
                text: qsTr("Repairing SSD and recovering videos: %1%2.").arg(phase_text(maintenance.phase)).arg(counts ? " (" + counts + ")" : "")
            };
        }
        if (DeviceRecording.error)
            return { kind: "critical", text: DeviceRecording.error };
        const sk = storage_kind;
        if (sk !== "ok")
            return { kind: sk === "safe" ? "safe" : sk, text: storage_explanation() };
        const camera = cameras.find(c => c.enabled && !is_recording(c.state));
        if (camera)
            return {
                kind: kind_for_camera(camera.state, true),
                text: qsTr("Camera %1 · %2: %3").arg(camera.slot + 1).arg(camera_name(camera)).arg(camera.reason || camera.state)
            };
        const lossy = cameras.find(c => c.enabled && c.state === "Recording with frame loss");
        if (lossy)
            return { kind: "caution", text: qsTr("Camera %1 · %2 is recording with frame loss.").arg(lossy.slot + 1).arg(camera_name(lossy)) };
        if (maintenance_result_pending) {
            const result = maintenance_result(maintenance);
            if (result)
                return { kind: result.kind, text: qsTr("%1. %2").arg(result.title).arg(result.text) };
        }
        if (cameras.length === 0)
            return { kind: "off", text: qsTr("Connect cameras. First use records H.265 1080p30, or MJPEG if H.265 is unavailable.") };
        return null;
    }

    // What to do about each storage state. The device's reason is appended where it
    // carries the specifics (which mount, which fault).
    function storage_explanation() {
        const reason = storage.reason && storage.reason !== storage.state ? storage.reason : "";
        const with_reason = text => reason ? text + " " + reason : text;
        switch (storage.state) {
        case "Safe to remove":
            return qsTr("All files finalized. You can unplug the SSD. To resume, open SSD details.");
        case "Waiting for SSD":
            return qsTr("No SSD available. Connect the recording SSD to the device's recording port.");
        case "Checking SSD":
            return qsTr("Checking the SSD. Recording starts when the check finishes.");
        case "Repairing SSD":
            return qsTr("Repairing the SSD. Recording starts after it is checked again.");
        case "Port not configured":
            return qsTr("The recording USB port has not been commissioned. Prepare an exFAT SSD on your PC before connecting it.");
        case "Selection required":
            return qsTr("More than one possible recording SSD is connected. Leave only the recording SSD connected.");
        case "Unsupported format":
            return qsTr("The SSD must be formatted as exFAT. Format it on a PC (this erases it), then reconnect it.");
        case "SSD busy":
            return with_reason(qsTr("The SSD is in use and can't be mounted for recording."));
        case "Repair required":
            return repair_enabled ? with_reason(qsTr("The SSD filesystem needs repair. Open SSD details and choose Repair SSD and recover videos.")) : with_reason(qsTr("The SSD filesystem needs repair. Repair isn't enabled on this device yet: copy important footage, repair the SSD on a PC, then reconnect it."));
        case "Write fault":
            return with_reason(qsTr("SSD recording stopped: the SSD can't be written."));
        case "Space fault":
            return qsTr("The recorder can't keep its free-space reserve. Remove other files from the SSD or use a larger SSD.");
        case "Storage operation failed":
            return with_reason(qsTr("The last SSD operation failed."));
        }
        return reason || storage.state || "";
    }

    // Exact storage.state values from videoserver recording/storage.py and manager.py.
    function kind_for_storage(state) {
        switch (state) {
        case "Ready":
            return "ok";
        case "Safe to remove":
            return "safe";
        case "Checking SSD":
        case "Repairing SSD":
            return "busy";
        case "Waiting for SSD":
            return "off";
        case "Port not configured":
        case "Selection required":
        case "Unsupported format":
        case "SSD busy":
            return "caution";
        case "Repair required":
        case "Write fault":
        case "Space fault":
        case "Storage operation failed":
            return "critical";
        }
        return "unknown";
    }

    // Exact camera state values. A camera can also inherit the storage state when the
    // SSD is unavailable, so anything unrecognised falls back to the storage mapping.
    function kind_for_camera(state, enabled) {
        if (!connected)
            return "unknown";
        if (!enabled || state === "Disabled")
            return "off";
        switch (state) {
        case "Recording":
            return "ok";
        case "Starting":
            return "busy";
        case "Recording with frame loss":
        case "Waiting for camera":
        case "Configuration required":
            return "caution";
        case "Write fault":
            return "critical";
        }
        return kind_for_storage(state);
    }

    // Events carry no severity, and "Recording interrupted" also covers intentional stops
    // (eject, config change, disabling), so most kinds stay neutral. Only outright failures
    // get caution; no event is shown as success proof for check/resume/repair.
    function kind_for_event(event) {
        switch (event ? event.kind : "") {
        case "Recording resumed":
        case "Configuration applied":
        case "Old file recovered":
            return "ok";
        case "SSD recovery completed":
            return event.failed > 0 ? "caution" : "ok";
        case "Configuration rollback":
        case "SSD operation failed":
        case "Old file recovery unavailable":
            return "caution";
        case "SSD recovery failed":
            return "critical";
        }
        return "off";
    }

    // Where an event happened: the camera, the targeted SSD (SSD operation events carry its
    // uuid since the maintenance API), or the device as a whole.
    function event_source(event) {
        const camera = event_camera(event);
        if (camera)
            return camera;
        if (event && event.uuid)
            return qsTr("SSD %1").arg(event.uuid);
        return qsTr("Device");
    }

    function event_details(event) {
        if (!event)
            return "";
        if (event.kind === "SSD recovery completed")
            return qsTr("%1 recovered, %2 failed").arg(event.recovered || 0).arg(event.failed || 0);
        return event.reason || "";
    }

    function is_recording(state) {
        return state === "Recording" || state === "Recording with frame loss";
    }

    function camera_name(camera) {
        return camera.name || qsTr("Camera %1").arg(camera.slot + 1);
    }

    function colour(kind) {
        switch (kind) {
        case "ok":
        case "safe":
            return Colour.status_ok;
        case "caution":
            return Colour.status_caution;
        case "critical":
            return Colour.status_critical;
        case "busy":
            return Colour.status_info;
        case "off":
            return Colour.text_muted;
        }
        return Colour.status_unknown;
    }

    function bytes(value) {
        const n = Number(value || 0);
        if (n >= 1e12)
            return qsTr("%1 TB").arg((n / 1e12).toFixed(2));
        if (n >= 1e9)
            return qsTr("%1 GB").arg(Math.round(n / 1e9));
        return qsTr("%1 MB").arg(Math.round(n / 1e6));
    }

    function profile_text(profile) {
        if (!profile || !profile.codec)
            return qsTr("Not configured");
        const codec = profile.codec === "h265" ? "H.265" : profile.codec === "jpeg" ? "MJPEG" : profile.codec.toUpperCase();
        const fps = profile.fps_d > 1 ? (profile.fps_n / profile.fps_d).toFixed(2) : profile.fps_n;
        return qsTr("%1  %2×%3  %4 fps").arg(codec).arg(profile.width).arg(profile.height).arg(fps);
    }

    function camera_for(stable_id) {
        return cameras.find(c => c.stable_id === stable_id) || null;
    }

    function time_text(seconds) {
        if (!seconds)
            return "";
        return new Date(Number(seconds) * 1000).toLocaleString(Qt.locale(), "yyyy-MM-dd  HH:mm:ss");
    }

    // Camera for an event: its slot and name, the raw identity when that camera is no
    // longer known (never an invented name), or "" for device-wide events.
    function event_camera(event) {
        if (!event || !event.camera)
            return "";
        const camera = camera_for(event.camera);
        if (camera)
            return qsTr("Camera %1 · %2").arg(camera.slot + 1).arg(camera_name(camera));
        return event.camera;
    }
}
