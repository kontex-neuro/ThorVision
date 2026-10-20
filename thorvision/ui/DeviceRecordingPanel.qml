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
                text: qsTr("Check SSD")
                enabled: DeviceRecording.connected && !DeviceRecording.busy && !!panel.storage.uuid
                onClicked: DeviceRecording.storage("check")
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
                model: panel.snapshot.cameras || []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    CheckBox {
                        text: modelData.name || qsTr("Camera %1").arg(modelData.slot + 1)
                        checked: !!modelData.enabled
                        enabled: DeviceRecording.connected && !DeviceRecording.busy
                        onClicked: DeviceRecording.set_enabled(modelData.stable_id, checked)
                        palette.windowText: Colour.text
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (DeviceRecording.connected ? modelData.state : qsTr("Unknown")) + " · " + (modelData.profile_source || "")
                        color: modelData.state === "Recording" ? Colour.text : "#F0C36A"
                        elide: Text.ElideRight
                        ToolTip.visible: statusMouse.containsMouse
                        ToolTip.text: modelData.reason || ""
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
