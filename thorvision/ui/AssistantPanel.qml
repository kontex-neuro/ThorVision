pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import App.Theme 0.1
import App.Extensions 1.0

// Analytics extensions for the selected camera: device connection, processed preview,
// candidate packages and their test reports, Apply / Stop / Roll Back, health and faults.
// Agent conversations that author packages connect through the agent bridge.
Rectangle {
    id: panel
    color: Colour.camera_settings
    // The bridge and the provider runtimes start only when the panel is first shown.
    onVisibleChanged: if (visible) Agents.start()

    readonly property var selected_item: CameraModel.rowCount > 0 ? CameraModel.get(CameraModel.selected_camera_index).camera_item : null
    property string camera_id: selected_item ? selected_item.stable_id : ""
    readonly property var info: {
        Extensions.state;
        return camera_id ? Extensions.camera(camera_id) : ({});
    }
    readonly property var run: info.run || null
    readonly property var active: info.active || null
    readonly property var previous: info.previous || null
    readonly property var fault: info.fault || null
    property string selected_digest: ""

    function short_digest(digest) { return digest ? digest.substring(0, 12) : ""; }
    function package_for(digest) {
        for (const item of Extensions.packages)
            if (item.digest === digest) return item;
        return null;
    }
    function state_colour(state) {
        return state === "Connected" ? Colour.success : state === "Connecting" ? Colour.accent1
             : state === "Disconnected" ? Colour.border_1 : Colour.warn;
    }

    Connections {
        target: CameraModel
        function onSelected_camera_changed() {
            panel.camera_id = panel.selected_item ? panel.selected_item.stable_id : panel.camera_id;
        }
    }

    FileDialog {
        id: upload_dialog
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Extension packages (*.tar.gz *.tgz *.tar)")]
        onAccepted: Extensions.upload_package(selectedFile)
    }

    component Caption: Label {
        font: AppFont.status_label
        color: Colour.border_3
    }
    component Value: Label {
        font: AppFont.status_value
        color: Colour.text
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    component PanelButton: Button {
        id: button
        font: AppFont.camera_settings_text
        contentItem: Label {
            text: button.text
            font: button.font
            color: button.enabled ? Colour.text : Colour.border_1
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            implicitHeight: 28
            radius: 3
            color: button.down ? Colour.down_background : button.hovered ? Colour.hovered_background : Colour.popup_button
            border.color: button.hovered ? Colour.hovered_border : Colour.border_2
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 10

            Label {
                text: qsTr("Analytics Assistant")
                font: AppFont.camera_settings_name
                color: Colour.text
            }

            // Device connection (SSH tunnel to the device-local extension service).
            GridLayout {
                columns: 2
                columnSpacing: 8
                rowSpacing: 4
                Layout.fillWidth: true

                Caption { text: qsTr("Device") }
                Value { text: Extensions.device_host }
                Caption { text: qsTr("SSH user") }
                TextField {
                    Layout.fillWidth: true
                    text: Extensions.ssh_user
                    enabled: !Extensions.connected && Extensions.connection_state !== "Connecting"
                    placeholderText: qsTr("device account")
                    onEditingFinished: Extensions.ssh_user = text
                }
                Caption { text: qsTr("Status") }
                RowLayout {
                    Layout.fillWidth: true
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: panel.state_colour(Extensions.connection_state)
                    }
                    Value { text: Extensions.connection_state }
                }
            }
            Value {
                visible: text.length > 0
                text: Extensions.connection_detail
                color: Colour.log_text
                font: AppFont.status_small
            }
            PanelButton {
                Layout.fillWidth: true
                text: Extensions.connected || Extensions.connection_state === "Connecting" ? qsTr("Disconnect") : qsTr("Connect")
                onClicked: Extensions.connected || Extensions.connection_state === "Connecting"
                           ? Extensions.disconnect_device() : Extensions.connect_device()
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Colour.border_1 }

            // Camera and processed preview.
            Caption { text: qsTr("Camera") }
            ComboBox {
                id: camera_box
                Layout.fillWidth: true
                model: { Extensions.state; CameraModel.rowCount; return Extensions.camera_ids(); }
                currentIndex: model.indexOf(panel.camera_id)
                onActivated: panel.camera_id = currentText
                displayText: panel.camera_id ? panel.camera_id.split("/").pop() : qsTr("No camera")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: width * 9 / 16
                color: Colour.video
                border.color: preview_item.fresh ? Colour.accent : Colour.video_border

                ProcessedPreviewItem {
                    id: preview_item
                    anchors.fill: parent
                    anchors.margins: 1
                    client: Extensions
                    camera: panel.camera_id
                }
                Label {
                    anchors.centerIn: parent
                    width: parent.width - 20
                    visible: !preview_item.fresh
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font: AppFont.status_small
                    color: panel.run && panel.run.state === "running" ? Colour.warn : Colour.border_3
                    text: panel.run && panel.run.state === "running"
                          ? qsTr("Analytics is running but no fresh processed preview arrived; showing raw video.")
                          : qsTr("No processed preview")
                }
            }

            // Active extension and health.
            GridLayout {
                columns: 2
                columnSpacing: 8
                rowSpacing: 3
                Layout.fillWidth: true

                Caption { text: qsTr("Active") }
                Value {
                    text: panel.active ? (panel.active.name || "") + "  " + panel.short_digest(panel.active.digest) : qsTr("None (raw preview)")
                }
                Caption { text: qsTr("State"); visible: !!panel.run }
                Value { visible: !!panel.run; text: panel.run ? panel.run.state : "" }
                Caption { text: qsTr("Rate"); visible: !!panel.run }
                Value {
                    visible: !!panel.run
                    text: panel.run ? qsTr("%1 / %2 FPS").arg(panel.run.processed_fps).arg(panel.run.target_fps) : ""
                    color: panel.run && panel.run.below_target ? Colour.warn : Colour.text
                }
                Caption { text: qsTr("Latency p95"); visible: !!panel.run && panel.run.latency_ms_p95 !== null }
                Value {
                    visible: !!panel.run && panel.run.latency_ms_p95 !== null
                    text: panel.run ? qsTr("%1 ms (callback %2 ms)").arg(panel.run.latency_ms_p95).arg(panel.run.callback_ms_p95) : ""
                }
                Caption { text: qsTr("Recording"); visible: !!Extensions.state.recorder_delivery }
                Value {
                    visible: !!Extensions.state.recorder_delivery
                    readonly property var delivery: Extensions.state.recorder_delivery || ({})
                    text: delivery.connected ? qsTr("Delivered to device recorder (%1 buffered)").arg(delivery.buffered || 0)
                                             : qsTr("Device recorder not receiving (%1 buffered)").arg(delivery.buffered || 0)
                    color: delivery.connected && !delivery.dropped ? Colour.text : Colour.warn
                }
                Caption { text: qsTr("Previous"); visible: !!panel.previous }
                Value {
                    visible: !!panel.previous
                    text: panel.previous ? (panel.previous.name || "") + "  " + panel.short_digest(panel.previous.digest) : ""
                }
            }

            Rectangle {
                visible: !!panel.fault
                Layout.fillWidth: true
                implicitHeight: fault_text.implicitHeight + 12
                color: "#3a1218"
                border.color: Colour.warn
                Label {
                    id: fault_text
                    anchors.fill: parent
                    anchors.margins: 6
                    wrapMode: Text.Wrap
                    font: AppFont.status_small
                    color: Colour.text
                    text: panel.fault ? qsTr("Fault (%1): %2").arg(panel.fault.kind).arg(panel.fault.message) : ""
                }
            }

            RowLayout {
                Layout.fillWidth: true
                PanelButton {
                    Layout.fillWidth: true
                    text: qsTr("Stop")
                    enabled: Extensions.connected && !!panel.active && !Extensions.busy
                    onClicked: Extensions.stop(panel.camera_id)
                }
                PanelButton {
                    Layout.fillWidth: true
                    text: qsTr("Roll Back")
                    enabled: Extensions.connected && !!panel.previous && !Extensions.busy
                    onClicked: Extensions.roll_back(panel.camera_id)
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Colour.border_1 }

            // Agent conversation through the local agent bridge. The agent edits the shared
            // extension project of this camera and tests it on the device; it cannot activate it.
            Binding { target: Agents; property: "camera"; value: panel.camera_id }

            RowLayout {
                Layout.fillWidth: true
                Caption { text: qsTr("Agent") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [{ id: "codex", name: "Codex" }, { id: "claude", name: "Claude" }, { id: "gemini", name: "Gemini" }]
                    textRole: "name"
                    valueRole: "id"
                    currentIndex: Math.max(0, ["codex", "claude", "gemini"].indexOf(Agents.agent))
                    enabled: !Agents.running
                    onActivated: Agents.agent = currentValue
                }
            }
            RowLayout {
                Layout.fillWidth: true
                readonly property var status: Agents.statuses[Agents.agent] || ({})
                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: parent.status.state === "connected" ? Colour.success
                         : !parent.status.state ? Colour.border_1 : Colour.warn
                }
                Value {
                    text: Agents.state !== "Ready" ? Agents.state + (Agents.detail ? ": " + Agents.detail : "")
                        : !parent.status.state ? qsTr("Checking account…")
                        : ({ connected: qsTr("Connected"), signin_required: qsTr("Sign-in required"),
                             quota_exhausted: qsTr("Usage limit reached"), unsupported: qsTr("Not usable in subscription mode"),
                             unavailable: qsTr("Not installed") })[parent.status.state] || parent.status.state
                    font: AppFont.status_small
                }
                PanelButton {
                    visible: !!parent.status.state && parent.status.state !== "connected"
                    text: parent.status.state === "signin_required" ? qsTr("Sign in") : qsTr("Recheck")
                    onClicked: parent.status.state === "signin_required" ? Agents.sign_in() : Agents.refresh_status(Agents.agent)
                }
            }
            Value {
                readonly property var status: Agents.statuses[Agents.agent] || ({})
                visible: !!status.state
                text: qsTr("Billing: %1").arg(status.billing || qsTr("Unknown"))
                      + (status.detail && status.state !== "connected" ? "\n" + status.detail : "")
                color: Colour.border_3
                font: AppFont.status_small
            }
            CustomCheckBox {
                text: qsTr("Ask before each tool use")
                checked: Agents.ask_before_tools
                onToggled: Agents.ask_before_tools = checked
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                color: Colour.log_background
                border.color: Colour.border_2

                ListView {
                    id: transcript
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    spacing: 6
                    model: Agents.transcript
                    onCountChanged: positionViewAtEnd()
                    delegate: Label {
                        required property var modelData
                        width: transcript.width
                        wrapMode: Text.Wrap
                        font: AppFont.status_small
                        textFormat: Text.PlainText
                        color: modelData.kind === "error" ? Colour.warn
                             : modelData.kind === "user" ? Colour.text
                             : modelData.kind === "agent" ? Colour.log_text : Colour.border_3
                        text: (modelData.kind === "user" ? "› " : modelData.kind === "tool" ? "⚙ " : "") + modelData.text
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: transcript.count === 0
                        width: parent.width - 20
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Colour.border_3
                        font: AppFont.status_small
                        text: qsTr("Describe the behavior, e.g. “Draw green boxes around people and show the number visible.”")
                    }
                }
            }

            Rectangle {
                visible: !!Agents.pending_permission.requestId
                Layout.fillWidth: true
                implicitHeight: permission_row.implicitHeight + 12
                color: Colour.hovered_background
                border.color: Colour.accent
                RowLayout {
                    id: permission_row
                    anchors.fill: parent
                    anchors.margins: 6
                    Value { text: qsTr("Allow %1?").arg(Agents.pending_permission.tool || "") }
                    PanelButton { text: qsTr("Allow"); onClicked: Agents.respond_permission(true) }
                    PanelButton { text: qsTr("Deny"); onClicked: Agents.respond_permission(false) }
                }
            }

            TextArea {
                id: prompt
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                wrapMode: TextEdit.Wrap
                placeholderText: qsTr("Describe what the analytics should do")
                enabled: !Agents.running
            }
            RowLayout {
                Layout.fillWidth: true
                PanelButton {
                    Layout.fillWidth: true
                    text: Agents.running ? qsTr("Working…") : qsTr("Send")
                    enabled: !Agents.running && prompt.text.trim().length > 0 && Extensions.connected
                             && (Agents.statuses[Agents.agent] || {}).state === "connected"
                    onClicked: {
                        Agents.send(prompt.text);
                        prompt.text = "";
                    }
                }
                PanelButton {
                    text: qsTr("Cancel")
                    enabled: Agents.running
                    onClicked: Agents.cancel()
                }
                PanelButton {
                    text: qsTr("New")
                    enabled: !Agents.running
                    onClicked: Agents.new_conversation()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Colour.border_1 }

            // Candidate packages on the device. Apply requires a passing test of the exact digest.
            RowLayout {
                Layout.fillWidth: true
                Caption { text: qsTr("Packages on device"); Layout.fillWidth: true }
                PanelButton {
                    text: qsTr("Upload…")
                    enabled: Extensions.connected && !Extensions.busy
                    onClicked: upload_dialog.open()
                }
            }
            Repeater {
                model: Extensions.packages
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property var report: modelData.report
                    readonly property bool passed: !!report && report.passed && report.digest === modelData.digest
                    Layout.fillWidth: true
                    implicitHeight: row_text.implicitHeight + 10
                    color: panel.selected_digest === modelData.digest ? Colour.hovered_background : "transparent"
                    border.color: panel.active && panel.active.digest === modelData.digest ? Colour.accent : Colour.border_2
                    Label {
                        id: row_text
                        anchors.fill: parent
                        anchors.margins: 5
                        wrapMode: Text.Wrap
                        font: AppFont.status_small
                        color: Colour.text
                        text: (row.modelData.manifest.name || "?") + " " + (row.modelData.manifest.version || "")
                              + "  " + panel.short_digest(row.modelData.digest) + "\n"
                              + (!row.report ? qsTr("Not tested")
                                 : row.passed ? qsTr("Test passed: %1 frames, est. %2 FPS max").arg(row.report.frames).arg(row.report.estimated_max_fps)
                                 : qsTr("Test failed: %1").arg((row.report.fault && row.report.fault.message) || row.report.error || ""))
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: panel.selected_digest = row.modelData.digest
                    }
                }
            }
            Value {
                visible: Extensions.connected && Extensions.packages.length === 0
                text: qsTr("No packages yet. Packages come from an agent session or Upload.")
                color: Colour.border_3
                font: AppFont.status_small
            }
            RowLayout {
                Layout.fillWidth: true
                readonly property var chosen: panel.package_for(panel.selected_digest)
                readonly property bool tested: !!chosen && !!chosen.report && chosen.report.passed && chosen.report.digest === chosen.digest
                PanelButton {
                    Layout.fillWidth: true
                    text: qsTr("Test")
                    enabled: Extensions.connected && !!parent.chosen && !Extensions.busy
                    onClicked: Extensions.test_package(panel.selected_digest)
                }
                PanelButton {
                    Layout.fillWidth: true
                    text: qsTr("Apply")
                    enabled: Extensions.connected && parent.tested && !!panel.camera_id && !Extensions.busy
                    onClicked: Extensions.apply(panel.camera_id, panel.selected_digest)
                }
            }

            BusyIndicator {
                visible: Extensions.busy
                running: visible
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 28
            }
            Value {
                visible: text.length > 0
                text: Extensions.message
                color: Colour.log_text
                font: AppFont.status_small
            }
        }
    }
}
