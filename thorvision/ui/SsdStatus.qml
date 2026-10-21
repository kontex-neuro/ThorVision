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

    // The one thing that needs attention now, or null. Priority: connection, operation
    // error, SSD, then the first selected camera that is not recording.
    readonly property var current_issue: {
        if (!connected)
            return {
                kind: "unknown",
                text: qsTr("Connection to the device lost. Reconnecting… This doesn't stop recording on the device.")
            };
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
            return with_reason(qsTr("The SSD filesystem needs repair. Repair it on a PC, then reconnect it."));
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
    function kind_for_event(kind) {
        switch (kind) {
        case "Recording resumed":
        case "Configuration applied":
        case "Old file recovered":
            return "ok";
        case "Configuration rollback":
        case "SSD operation failed":
        case "Old file recovery unavailable":
            return "caution";
        }
        return "off";
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
