import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App.Theme 0.1

Rectangle {
    id: panel
    color: Colour.background_1
    border.color: Colour.border_1
    implicitHeight: content.implicitHeight + 16
    property var snapshot: DeviceRecording.state
    property var storage: snapshot.storage || ({})
    function space(bytes) { return (Number(bytes || 0) / 1000000000).toFixed(1) + " GB" }
    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Device SSD recording"); font.bold: true; color: Colour.text }
            Label {
                Layout.fillWidth: true
                text: DeviceRecording.connected ? (panel.storage.state || "") + "  |  " + panel.space(panel.storage.free_bytes) + qsTr(" free · exFAT") + (panel.snapshot.estimated_history_seconds ? qsTr(" · estimated history %1 h").arg((panel.snapshot.estimated_history_seconds/3600).toFixed(1)) : "") : qsTr("Device disconnected — SSD recording status unknown")
                color: Colour.text
                elide: Text.ElideRight
            }
            Label { text: qsTr("File length (s)"); color: Colour.text }
            SpinBox {
                from: 10; to: 3600; value: panel.snapshot.segment_seconds || 60
                enabled: DeviceRecording.connected && !DeviceRecording.busy
                onValueModified: DeviceRecording.set_segment_seconds(value)
                Accessible.name: qsTr("Device recording file length in seconds")
            }
            Button {
                text: panel.storage.state === "Safe to remove" ? qsTr("Resume") : qsTr("Safely eject SSD")
                enabled: DeviceRecording.connected && !DeviceRecording.busy && !!panel.storage.uuid
                onClicked: DeviceRecording.storage(panel.storage.state === "Safe to remove" ? "resume" : "eject")
            }
            Button {
                objectName: "ssdDetailsButton"
                text: qsTr("SSD details...")
                onClicked: ssd_details.open()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("View SSD and recording status. Recording continues.")
            }
            Button {
                text: qsTr("Repair SSD…")
                visible: panel.storage.state === "Repair required"
                enabled: !!panel.storage.repair_enabled && !DeviceRecording.busy
                onClicked: repair.open()
                ToolTip.visible: hovered
                ToolTip.text: panel.storage.repair_enabled ? qsTr("Repair the recording filesystem") : qsTr("Device repair awaits POC approval; repair this SSD on a PC.")
            }
        }
        GridLayout {
            columns: 4
            Layout.fillWidth: true
            Repeater {
                model: (panel.snapshot.cameras || []).length
                delegate: RowLayout {
                    objectName: "deviceRecordingCameraRow"
                    required property int index
                    readonly property var cameraState: (panel.snapshot.cameras || [])[index] || ({})
                    Layout.fillWidth: true
                    CheckBox {
                        text: cameraState.name || qsTr("Camera %1").arg(cameraState.slot + 1)
                        checked: !!cameraState.enabled
                        enabled: DeviceRecording.connected && !DeviceRecording.busy
                        onClicked: DeviceRecording.set_enabled(cameraState.stable_id, checked)
                        palette.windowText: Colour.text
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (DeviceRecording.connected ? cameraState.state : qsTr("Unknown")) + " · " + (cameraState.profile_source || "")
                        color: cameraState.state === "Recording" ? Colour.text : "#F0C36A"
                        elide: Text.ElideRight
                        ToolTip.visible: statusMouse.containsMouse
                        ToolTip.text: cameraState.reason || ""
                        MouseArea { id: statusMouse; anchors.fill: parent; hoverEnabled: true }
                    }
                }
            }
            Label {
                visible: !(panel.snapshot.cameras || []).length
                text: qsTr("Connect cameras. First-use profile: H.265 1080p30; MJPEG fallback.")
                color: Colour.text
            }
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: DeviceRecording.error || panel.storage.reason || (panel.storage.state === "Port not configured" ? qsTr("The recording USB port has not been commissioned. Prepare an exFAT SSD on your PC before connecting it.") : "")
            color: "#F0C36A"
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            property var event: (panel.snapshot.events || []).length ? panel.snapshot.events[panel.snapshot.events.length - 1] : ({})
            text: event.kind ? qsTr("Latest event: ") + event.kind + "  " + new Date(Number(event.at) * 1000).toLocaleTimeString() : ""
            visible: text.length > 0
            color: Colour.text
            elide: Text.ElideRight
        }
    }
    Dialog {
        id: ssd_details
        objectName: "ssdDetailsDialog"
        title: qsTr("SSD status")
        width: 560
        modal: true
        anchors.centerIn: Overlay.overlay
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: qsTr("Viewing this information does not interrupt recording.")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: !DeviceRecording.connected
                text: qsTr("Device disconnected. Values below are from the last received status.")
                wrapMode: Text.WordWrap
            }
            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 8
                Layout.fillWidth: true
                Label { text: qsTr("SSD state") }
                Label { text: DeviceRecording.connected ? (panel.storage.state || qsTr("Unknown")) : qsTr("Unknown") }
                Label { text: qsTr("SSD recording") }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: {
                        if (!DeviceRecording.connected) return qsTr("Unknown");
                        const cameras = panel.snapshot.cameras || [];
                        let selected = 0, recording = 0;
                        for (const camera of cameras) {
                            if (camera.enabled) {
                                ++selected;
                                if (camera.state === "Recording" || camera.state === "Recording with frame loss") ++recording;
                            }
                        }
                        return qsTr("%1 of %2 selected cameras recording").arg(recording).arg(selected);
                    }
                }
                Label { text: qsTr("Free / total space") }
                Label { text: panel.storage.total_bytes ? panel.space(panel.storage.free_bytes) + " / " + panel.space(panel.storage.total_bytes) : qsTr("Unknown") }
                Label { text: qsTr("Reserved free space") }
                Label { text: panel.storage.total_bytes ? panel.space(panel.storage.reserve_bytes) : qsTr("Unknown") }
                Label { text: qsTr("Estimated recording history") }
                Label { text: panel.snapshot.estimated_history_seconds ? qsTr("About %1 h at the current rate").arg((panel.snapshot.estimated_history_seconds / 3600).toFixed(1)) : qsTr("Available while recording") }
                Label { text: qsTr("Volume ID") }
                Label { text: panel.storage.uuid || qsTr("Unavailable"); textFormat: Text.PlainText }
            }
            Label {
                Layout.fillWidth: true
                property var latest: (panel.snapshot.events || []).length ? panel.snapshot.events[panel.snapshot.events.length - 1] : ({})
                text: latest.kind ? qsTr("Latest event: %1 (%2)").arg(latest.kind).arg(new Date(Number(latest.at) * 1000).toLocaleTimeString()) : qsTr("No recording events yet.")
                wrapMode: Text.WordWrap
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: ssd_details.palette.mid }
            Label {
                Layout.fillWidth: true
                text: qsTr("Filesystem check pauses SSD recording. Review the recording impact before continuing.")
                wrapMode: Text.WordWrap
            }
        }
        footer: DialogButtonBox {
            Button {
                objectName: "openFilesystemCheckButton"
                text: qsTr("Check filesystem...")
                enabled: DeviceRecording.connected && !DeviceRecording.busy && !!panel.storage.uuid
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: { ssd_details.close(); filesystem_check.open(); }
            }
            Button { text: qsTr("Close"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
    Dialog {
        id: filesystem_check
        objectName: "filesystemCheckDialog"
        title: qsTr("Pause SSD recording and check filesystem?")
        width: 560
        modal: true
        anchors.centerIn: Overlay.overlay
        property string volumeId: ""
        property string attachment: ""
        readonly property bool sameAttachment: DeviceRecording.connected && volumeId.length > 0 && volumeId === (panel.storage.uuid || "") && attachment === (panel.storage.generation || "")
        onAboutToShow: { volumeId = panel.storage.uuid || ""; attachment = panel.storage.generation || ""; }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: qsTr("SSD recording will pause."); font.bold: true; color: Colour.warn }
            Label { text: qsTr("Volume: %1").arg(filesystem_check.volumeId); textFormat: Text.PlainText }
            Label {
                Layout.fillWidth: true
                text: qsTr("Current SSD recording files will be finalized and the volume unmounted for a read-only filesystem check. Footage during the pause will not be recorded to the SSD.")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("If the check passes, the volume will mount again and SSD recording will resume in new files. Existing videos are preserved. This does not reset, format or repair the SSD. Camera preview and PC recording can continue.")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: !filesystem_check.sameAttachment
                text: qsTr("The SSD connection changed or the device is offline. Close this dialog and review its current status first.")
                color: Colour.warn
                wrapMode: Text.WordWrap
            }
        }
        footer: DialogButtonBox {
            Button {
                objectName: "confirmFilesystemCheckButton"
                text: qsTr("Pause SSD recording and check")
                enabled: filesystem_check.sameAttachment && !DeviceRecording.busy
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: { if (sameAttachment && !DeviceRecording.busy) DeviceRecording.storage("check"); }
    }
    Dialog {
        id: repair
        title: qsTr("Repair recording SSD?")
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 480
            wrapMode: Text.WordWrap
            text: qsTr("SSD %1\nLocal recording will pause. Filesystem repair can change or remove damaged files. Copy important footage first. PC recording can continue while its camera stream remains available.").arg(panel.storage.uuid || "")
        }
        onAccepted: DeviceRecording.storage("repair", true)
    }
}
