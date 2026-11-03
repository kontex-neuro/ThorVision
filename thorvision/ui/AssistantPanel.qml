pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import App.Theme 0.1

// Author and test analytics for the selected main-view camera. Chat keeps its editor
// visible; extension controls and connection details have separate pages.
// The processed preview lives in VideoPreview alongside its raw-video fallback.
Rectangle {
    id: panel
    color: Colour.camera_settings
    // The bridge and the provider runtimes start only when the panel is first shown.
    onVisibleChanged: if (visible) Agents.start()

    readonly property var selected_item: CameraModel.rowCount > 0 ? CameraModel.get(CameraModel.selected_camera_index).camera_item : null
    readonly property string camera_id: selected_item ? selected_item.stable_id : ""
    readonly property var info: {
        Extensions.state;
        return camera_id ? Extensions.camera(camera_id) : ({});
    }
    readonly property var run: info.run || null
    readonly property var active: info.active || null
    readonly property var previous: info.previous || null
    readonly property var fault: info.fault || null
    readonly property var preparation: {
        if (!Extensions.connected) return null;
        const jobs = Extensions.state.preparation_jobs || [];
        return jobs.find(job => job.kind === "prepare" && job.state === "running") || null;
    }
    property string selected_digest: ""
    readonly property bool can_send: !Agents.running && !!camera_id && Extensions.connected
                                    && Agents.state === "Ready"
                                    && (Agents.statuses[Agents.agent] || {}).state === "connected"

    function send_prompt() {
        if (!can_send || !prompt.text.trim().length) return;
        Agents.send(prompt.text);
        prompt.clear();
    }

    function short_digest(digest) { return digest ? digest.substring(0, 12) : ""; }
    function package_for(digest) {
        for (const item of Extensions.packages)
            if (item.digest === digest) return item;
        return null;
    }
    function state_colour(state) {
        return state === "Connected" ? Colour.success : state === "Connecting" ? Colour.accent1
             : state === "Disconnected" ? Colour.border_1 : Colour.status_caution;
    }

    FileDialog {
        id: upload_dialog
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Extension packages (*.tar.gz *.tgz *.tar)")]
        onAccepted: Extensions.upload_package(selectedFile)
    }

    Connections {
        target: Extensions
        function onCleanup_plan_changed() {
            if (Extensions.cleanup_plan.id) cleanup_dialog.open();
        }
    }
    Dialog {
        id: cleanup_dialog
        title: qsTr("Review package cleanup")
        modal: true
        anchors.centerIn: parent
        width: Math.min(panel.width - 20, 540)
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Remove %1 unused items and reclaim approximately %2 MiB? Active and rollback packages, recordings and shared dependencies still in use are kept.")
                    .arg((Extensions.cleanup_plan.items || []).length)
                    .arg(((Extensions.cleanup_plan.total_bytes || 0) / 1048576).toFixed(1))
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                TextArea {
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    text: (Extensions.cleanup_plan.items || []).map(item => item.kind + ": " + item.id).join("\n")
                        + ((Extensions.cleanup_plan.blocked || []).length ? "\n\n" + qsTr("Protected:") + "\n"
                           + Extensions.cleanup_plan.blocked.map(item => item.digest + ": " + item.reasons.join(", ")).join("\n") : "")
                }
            }
            Button {
                text: qsTr("Delete these unused items")
                enabled: (Extensions.cleanup_plan.items || []).length > 0 && (Extensions.cleanup_plan.blocked || []).length === 0
                onClicked: { cleanup_dialog.close(); Extensions.execute_cleanup(); }
            }
        }
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10
        Label {
            text: qsTr("Analytics Assistant")
            font: AppFont.camera_settings_name
            color: Colour.text
        }
        ComboBox {
            Layout.fillWidth: true
            model: {
                CameraModel.rowCount;
                let names = [];
                for (let i = 0; i < CameraModel.rowCount; ++i)
                    names.push(CameraModel.get(i).camera_item.name);
                return names;
            }
            currentIndex: CameraModel.selected_camera_index
            displayText: panel.selected_item ? panel.selected_item.name : qsTr("No camera")
            enabled: !Agents.running
            onActivated: CameraModel.selected_camera_index = currentIndex
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font: AppFont.status_small
            color: Colour.text_muted
            text: Extensions.connected
                  ? qsTr("Device connected. Watch the result in the main video.")
                  : qsTr("Connecting to device analytics automatically…")
        }
        TabBar {
            id: tabs
            objectName: "assistant_tabs"
            Layout.fillWidth: true
            TabButton { text: qsTr("Chat") }
            TabButton { text: qsTr("Extensions") }
            TabButton { text: qsTr("Device") }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            ColumnLayout {
                spacing: 8
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
                             : !parent.status.state ? Colour.border_1 : Colour.status_caution
                    }
                    Value {
                        text: Agents.state !== "Ready" ? Agents.state + (Agents.detail ? ": " + Agents.detail : "")
                            : !parent.status.state ? qsTr("Checking account…")
                            : ({ connected: qsTr("Agent connected"), signin_required: qsTr("Sign-in required"),
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
                    visible: !!panel.preparation
                    font: AppFont.status_small
                    color: Colour.status_info
                    text: !panel.preparation ? ""
                        : panel.preparation.stage === "downloading"
                            ? qsTr("Downloading through this PC: %1 MiB").arg(((panel.preparation.download_bytes || 0) / 1048576).toFixed(1))
                        : panel.preparation.stage === "installing_offline" ? qsTr("Preparing isolated environment…")
                        : qsTr("Connecting the preparation proxy…")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Caption { text: qsTr("Conversation"); Layout.fillWidth: true }
                    PanelButton {
                        objectName: "assistant_copy_all"
                        text: qsTr("Copy all")
                        enabled: transcript.length > 0
                        onClicked: {
                            const start = transcript.selectionStart;
                            const end = transcript.selectionEnd;
                            transcript.selectAll();
                            transcript.copy();
                            transcript.select(start, end);
                        }
                    }
                }
                ScrollView {
                    id: transcript_scroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 80
                    contentWidth: availableWidth
                    clip: true
                    background: Rectangle { color: Colour.log_background; border.color: Colour.border_1; radius: 3 }
                    TextArea {
                        id: transcript
                        objectName: "assistant_transcript"
                        readOnly: true
                        selectByMouse: true
                        persistentSelection: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        font: AppFont.status_small
                        color: Colour.text
                        selectionColor: Colour.accent1
                        selectedTextColor: Colour.text
                        padding: 10
                        background: null
                        text: Agents.transcript.map(function(entry) {
                            const name = entry.kind === "user" ? qsTr("You")
                                       : entry.kind === "agent" ? qsTr("Assistant")
                                       : entry.kind === "error" ? qsTr("Error") : qsTr("Activity");
                            return name + "\n" + entry.text;
                        }).join("\n\n")
                        placeholderText: qsTr("Describe what you want to see in the video. The assistant will build and test it; you choose when to apply it.")
                        placeholderTextColor: Colour.text_muted
                        // Follow output only while the reader is already at the bottom and not selecting text.
                        property bool follow_output: true
                        onTextChanged: if (follow_output && !selectedText.length) Qt.callLater(function() {
                            transcript_scroll.contentItem.contentY = Math.max(0, transcript_scroll.contentItem.contentHeight - transcript_scroll.availableHeight);
                        })
                        Connections {
                            target: transcript_scroll.contentItem
                            function onContentYChanged() {
                                transcript.follow_output = transcript_scroll.contentItem.contentY >=
                                    transcript_scroll.contentItem.contentHeight - transcript_scroll.availableHeight - 24;
                            }
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

                Label {
                    text: qsTr("Your prompt")
                    color: Colour.text
                    font: AppFont.status_small
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(100, Math.min(180, panel.height * 0.22))
                    contentWidth: availableWidth
                    clip: true
                    background: Rectangle {
                        color: Colour.log_background
                        border.color: prompt.activeFocus ? Colour.status_info : Colour.border_1
                        border.width: prompt.activeFocus ? 2 : 1
                        radius: 3
                    }
                    TextArea {
                        id: prompt
                        objectName: "assistant_prompt"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        color: Colour.text
                        selectionColor: Colour.accent1
                        selectedTextColor: Colour.text
                        placeholderTextColor: Colour.text_muted
                        font: AppFont.status_small
                        padding: 10
                        background: null
                        placeholderText: qsTr("For example: Draw a green box around each person and show the count.")
                        Keys.onPressed: function(event) {
                            if ((event.modifiers & Qt.ControlModifier) &&
                                (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                                panel.send_prompt();
                                event.accepted = true;
                            }
                        }
                    }
                }
                Value {
                    font: AppFont.status_small
                    color: Colour.text_muted
                    text: !panel.camera_id ? qsTr("Select a camera to start.")
                        : !Extensions.connected ? qsTr("Waiting for device analytics. Connection is automatic.")
                        : (Agents.statuses[Agents.agent] || {}).state !== "connected" ? qsTr("Connect your agent account to send a prompt.")
                        : Agents.running ? qsTr("Working… You can draft your next prompt below.")
                        : qsTr("Enter for a new line · Ctrl+Enter to send")
                }
                RowLayout {
                    Layout.fillWidth: true
                    PanelButton {
                        objectName: "assistant_send"
                        Layout.fillWidth: true
                        text: Agents.running ? qsTr("Working…") : qsTr("Send")
                        enabled: panel.can_send && prompt.text.trim().length > 0
                        onClicked: panel.send_prompt()
                    }
                    PanelButton {
                        text: qsTr("Cancel")
                        enabled: Agents.running
                        onClicked: Agents.cancel()
                    }
                    PanelButton {
                        text: qsTr("New chat")
                        enabled: !Agents.running
                        onClicked: Agents.new_conversation()
                    }
                }

            }
            ScrollView {
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
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
                            color: panel.run && panel.run.below_target ? Colour.status_caution : Colour.text
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
                            color: delivery.connected && !delivery.dropped ? Colour.text : Colour.status_caution
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
                        border.color: Colour.status_caution
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

                    // Candidate packages on the device. Apply requires a passing test of the exact digest.
                    RowLayout {
                        Layout.fillWidth: true
                        Caption { text: qsTr("Packages on device"); Layout.fillWidth: true }
                        PanelButton {
                            text: qsTr("Upload…")
                            enabled: Extensions.connected && !Extensions.busy && !Agents.running
                            onClicked: upload_dialog.open()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        readonly property var chosen: panel.package_for(panel.selected_digest)
                        readonly property bool tested: !!chosen && chosen.ready === true
                        PanelButton {
                            Layout.fillWidth: true
                            text: qsTr("Prepare")
                            enabled: Extensions.connected && !!parent.chosen && !Extensions.busy && !Agents.running
                            onClicked: Agents.prepare_package(panel.selected_digest)
                        }
                        PanelButton {
                            Layout.fillWidth: true
                            text: qsTr("Apply")
                            enabled: Extensions.connected && parent.tested && !!panel.camera_id && !Extensions.busy && !Agents.running
                            onClicked: Extensions.apply(panel.camera_id, panel.selected_digest)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        PanelButton {
                            text: qsTr("Delete selected…")
                            enabled: Extensions.connected && !Extensions.busy && !Agents.running
                                     && !!panel.package_for(panel.selected_digest)
                                     && panel.package_for(panel.selected_digest).can_delete === true
                            onClicked: Extensions.plan_cleanup([panel.selected_digest])
                        }
                        PanelButton {
                            text: qsTr("Clean unused…")
                            enabled: Extensions.connected && !Extensions.busy && !Agents.running
                            onClicked: Extensions.plan_cleanup(Extensions.packages.filter(item => item.can_delete === true).map(item => item.digest))
                        }
                    }
                    Value {
                        readonly property var chosen: panel.package_for(panel.selected_digest)
                        visible: !!chosen && (chosen.protected_reasons || []).length > 0
                        text: chosen ? qsTr("Kept: %1").arg((chosen.protected_reasons || []).join(", ")) : ""
                        font: AppFont.status_small
                    }
                    Repeater {
                        model: Extensions.packages
                        delegate: Rectangle {
                            id: row
                            required property var modelData
                            readonly property var report: modelData.report
                            readonly property bool passed: modelData.ready === true
                            Layout.fillWidth: true
                            implicitHeight: row_text.implicitHeight + 10
                            color: panel.selected_digest === modelData.digest ? Colour.hovered_background : "transparent"
                            border.color: panel.active && panel.active.digest === modelData.digest ? Colour.accent : Colour.border_2
                            Label {
                                id: row_text
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                anchors.fill: parent
                                anchors.margins: 5
                                wrapMode: Text.Wrap
                                font: AppFont.status_small
                                color: Colour.text
                                text: (row.modelData.manifest.name || "?") + " " + (row.modelData.manifest.version || "")
                                      + "  " + panel.short_digest(row.modelData.digest) + "\n"
                                      + (!row.report ? qsTr("Not prepared")
                                         : row.passed ? qsTr("Ready to apply: %1 test frames, estimated %2 FPS maximum").arg(row.report.frames).arg(row.report.estimated_max_fps)
                                         : row.report.passed ? qsTr("Needs preparation for the current device runtime")
                                         : qsTr("Preparation blocked: %1").arg((row.report.fault && row.report.fault.message) || row.report.error || ""))
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
            ScrollView {
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
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

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        Caption { text: qsTr("Device") }
                        Value { text: Extensions.device_host }
                        Caption { text: qsTr("Analytics") }
                        Value { text: Extensions.connection_state }
                    }
                    Value {
                        text: Extensions.connection_detail
                        font: AppFont.status_small
                        color: Colour.log_text
                    }
                    Value {
                        text: qsTr("Analytics connects automatically through the camera's device API. No separate device sign-in is required.")
                        font: AppFont.status_small
                    }
                    Value {
                        text: qsTr("Preparation automatically checks the package, reuses compatible dependencies, downloads missing files through this PC, and tests the result. Apply stays under your control.")
                        font: AppFont.status_small
                    }
                    PanelButton {
                        visible: !Extensions.connected
                        text: qsTr("Retry now")
                        onClicked: Extensions.connect_device()
                    }
                }
            }
        }
    }
}
