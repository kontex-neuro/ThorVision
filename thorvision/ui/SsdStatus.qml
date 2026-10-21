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

    readonly property var latest_event: snapshot.events && snapshot.events.length ? snapshot.events[snapshot.events.length - 1] : null

    // True when the latest event is what caused the current issue (same camera), so the
    // details dialog can show them as one line instead of repeating it.
    readonly property bool issue_is_latest_event: !!current_issue && !!current_issue.stable_id && !!latest_event && latest_event.stable_id === current_issue.stable_id

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
                stable_id: camera.stable_id,
                text: qsTr("Camera %1 · %2: %3").arg(camera.slot + 1).arg(camera_name(camera)).arg(camera.reason || camera.state)
            };
        const lossy = cameras.find(c => c.enabled && c.state === "Recording with frame loss");
        if (lossy)
            return { kind: "caution", text: qsTr("Camera %1 · %2 is recording with frame loss.").arg(lossy.slot + 1).arg(camera_name(lossy)) };
        if (cameras.length === 0)
            return { kind: "off", text: qsTr("Connect cameras. First use records H.265 1080p30, or MJPEG if H.265 is unavailable.") };
        return null;
    }

    function storage_explanation() {
        if (storage.reason)
            return storage.reason;
        switch (storage_kind) {
        case "safe":
            return qsTr("All files finalized. You can unplug the SSD. To resume, open SSD details.");
        case "off":
            return qsTr("No SSD available. Connect the recording SSD to the device's recording port.");
        case "busy":
            return qsTr("%1 SSD recording starts or resumes when this finishes.").arg(storage.state + ".");
        case "critical":
            return qsTr("SSD recording stopped. Open SSD details for the reason.");
        }
        if (storage.state === "Port not configured")
            return qsTr("The recording USB port has not been commissioned. Prepare an exFAT SSD on your PC before connecting it.");
        return storage.state || "";
    }

    function kind_for_storage(state) {
        const s = (state || "").toLowerCase();
        if (s === "ready")
            return "ok";
        if (s === "safe to remove")
            return "safe";
        if (s.indexOf("error") >= 0 || s.indexOf("repair") >= 0 || s.indexOf("fail") >= 0 || s.indexOf("read-only") >= 0 || s.indexOf("full") >= 0)
            return "critical";
        if (s.indexOf("check") >= 0 || s.indexOf("prepar") >= 0 || s.indexOf("validat") >= 0 || s.indexOf("mount") >= 0 || s.indexOf("eject") >= 0 || s.indexOf("recover") >= 0 || s.indexOf("resum") >= 0)
            return "busy";
        if (s === "" || s.indexOf("not detected") >= 0 || s.indexOf("missing") >= 0 || s.indexOf("no ssd") >= 0 || s.indexOf("absent") >= 0)
            return "off";
        return "caution";
    }

    function kind_for_camera(state, enabled) {
        if (!connected)
            return "unknown";
        if (!enabled)
            return "off";
        const s = (state || "").toLowerCase();
        if (s === "recording")
            return "ok";
        if (s.indexOf("start") >= 0 || s.indexOf("recover") >= 0 || s.indexOf("wait") >= 0 || s.indexOf("finaliz") >= 0)
            return "busy";
        if (s.indexOf("error") >= 0 || s.indexOf("fail") >= 0)
            return "critical";
        return "caution";
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

    // "Camera 2 · Arena side recording interrupted: camera disconnected." when the event
    // carries a camera and reason, otherwise the event kind on its own.
    function event_text(event) {
        if (!event)
            return qsTr("No recording events yet.");
        if (event.message)
            return event.message;
        const camera = event.stable_id ? camera_for(event.stable_id) : null;
        let text = camera ? qsTr("Camera %1 · %2 %3").arg(camera.slot + 1).arg(camera_name(camera)).arg(event.kind) : event.kind;
        if (event.reason)
            text += ": " + event.reason;
        return text;
    }
}
