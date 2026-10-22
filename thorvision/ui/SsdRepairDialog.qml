pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// "Repair SSD and recover videos": one confirmed device job that backs up interrupted
// recordings, recovers them, repairs the exFAT filesystem and requests recording to resume.
// Views: confirm -> job (progress while running, result when finished). The job runs on the
// device; closing this dialog or ThorVision does not cancel it.
AlertDialog {
    id: dialog

    objectName: "repairDialog"

    // "confirm" or "job".
    property string view: "confirm"
    // Job id the device reported before we asked for a new one, to tell the new job apart.
    property string previous_job: ""
    property bool waiting_for_job: false

    property string volume_id: ""
    property string attachment: ""

    readonly property var storage: SsdStatus.storage
    readonly property var job: SsdStatus.maintenance
    readonly property bool same_attachment: SsdStatus.connected && volume_id.length > 0 && volume_id === (storage.uuid || "") && attachment === (storage.generation || "")
    readonly property bool job_started: !!job && job.id !== previous_job
    readonly property bool start_failed: waiting_for_job && !job_started && !DeviceRecording.busy && !!DeviceRecording.error
    readonly property var job_result: view === "job" && job_started && !SsdStatus.maintenance_active ? SsdStatus.maintenance_result(job) : null

    // Opens on the current job if there is one to look at, otherwise on the confirmation.
    function show() {
        waiting_for_job = false;
        previous_job = "";
        view = SsdStatus.current_maintenance_active || SsdStatus.maintenance_result_pending ? "job" : "confirm";
        open();
    }

    onJob_startedChanged: if (job_started) waiting_for_job = false

    onAboutToShow: {
        volume_id = storage.uuid || "";
        attachment = storage.generation || "";
    }

    title_text: {
        if (view === "confirm")
            return qsTr("Repair SSD and recover videos?");
        if (job_result)
            return job_result.title;
        return qsTr("Repairing SSD and recovering videos");
    }
    icon_source: job_result && job_result.kind === "ok" ? "qrc:/qt/qml/App/Theme/resources/checked.svg" : "qrc:/qt/qml/App/Theme/resources/warning.svg"
    footer_left_margin: 40
    footer_right_margin: 40
    footer_bottom_margin: 26

    component Banner: Rectangle {
        id: banner

        property string kind: "busy"
        property alias text: banner_text.text

        radius: 3
        border.width: 1
        color: kind === "caution" ? Colour.caution_panel : kind === "critical" ? Colour.critical_panel : kind === "ok" ? Colour.background_1 : Colour.info_panel
        border.color: kind === "caution" ? Colour.caution_panel_border : kind === "critical" ? Colour.critical_panel_border : kind === "ok" ? Colour.border_2 : Colour.info_panel_border

        Layout.fillWidth: true
        Layout.preferredHeight: banner_text.implicitHeight + 16

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            StatusIcon {
                kind: banner.kind === "ok" ? "safe" : banner.kind

                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 1
            }

            Label {
                id: banner_text

                textFormat: Text.StyledText
                font: AppFont.status_text
                color: Colour.text
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }
        }
    }

    content_data: Item {
        anchors.fill: parent

        // ---------------------------------------------------------------- confirm
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 40
            anchors.rightMargin: 40
            anchors.topMargin: 18
            spacing: 12
            visible: dialog.view === "confirm"

            Rectangle {
                color: Colour.background_1
                border.color: Colour.border_2
                border.width: 1
                radius: 3

                Layout.fillWidth: true
                Layout.preferredHeight: 34

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14

                    Label {
                        text: qsTr("SSD %1 · %2 · %3").arg(dialog.volume_id || qsTr("unknown")).arg(dialog.storage.total_bytes ? SsdStatus.bytes(dialog.storage.total_bytes) : "–").arg(SsdStatus.storage_state)
                        textFormat: Text.PlainText
                        font: AppFont.status_text
                        color: Colour.text
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("%1 cameras recording").arg(SsdStatus.recording_count)
                        font: AppFont.status_text
                        color: Colour.text_muted
                    }
                }
            }

            Banner {
                visible: !dialog.same_attachment || !SsdStatus.repair_enabled
                kind: "critical"
                text: !SsdStatus.repair_enabled ? qsTr("<b>SSD repair is unavailable on this device.</b> Copy important footage and repair the SSD on a PC instead.") : SsdStatus.connected ? qsTr("<b>The SSD changed while this dialog was open.</b> It was disconnected or replaced, so the repair can't start. Review the current SSD status first.") : qsTr("<b>The device is offline.</b> The repair can't start.")
            }

            ColumnLayout {
                spacing: 9
                opacity: dialog.same_attachment && SsdStatus.repair_enabled ? 1 : 0.55

                Layout.fillWidth: true

                Repeater {
                    model: [
                        { kind: "caution", text: qsTr("<b>SSD recording pauses.</b> Current files are finalized first.") },
                        { kind: "off", text: qsTr("Readable interrupted recordings are backed up on the device, then video recovery is attempted.") },
                        { kind: "caution", text: qsTr("Filesystem repair <b>can modify damaged files</b>. It does not format the SSD.") },
                        { kind: "critical", text: qsTr("Footage during the pause <b>can't be recovered</b>.") },
                        { kind: "ok", text: qsTr("Recording resumes in new files if the SSD becomes ready.") },
                        { kind: "ok", text: qsTr("Camera preview and PC recording continue while their camera is available.") }
                    ]

                    delegate: RowLayout {
                        id: fact

                        required property var modelData

                        spacing: 12
                        Layout.fillWidth: true

                        StatusIcon {
                            kind: fact.modelData.kind
                            Layout.preferredWidth: 13
                            Layout.preferredHeight: 13
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 2
                        }

                        Label {
                            text: fact.modelData.text
                            textFormat: Text.StyledText
                            font: AppFont.status_title
                            color: Colour.text
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Label {
                text: qsTr("Only recorder files are backed up, not the whole SSD. If other data on the SSD matters, recover it separately before repairing.")
                font: AppFont.status_small
                color: Colour.text_muted
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // ---------------------------------------------------------------- job
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 40
            anchors.rightMargin: 40
            anchors.topMargin: 18
            spacing: 12
            visible: dialog.view === "job"

            Banner {
                kind: dialog.job_result ? dialog.job_result.kind : dialog.start_failed ? "critical" : "busy"
                text: {
                    if (dialog.start_failed)
                        return qsTr("<b>The repair didn't start.</b> %1").arg(DeviceRecording.error);
                    if (dialog.job_result)
                        return dialog.job_result.text;
                    return qsTr("SSD recording is paused while the device works. You can close this window: the repair continues on the device, even if ThorVision closes.");
                }
            }

            // Phases while running (or waiting for the device to accept the job).
            ColumnLayout {
                spacing: 9
                visible: !dialog.job_result && !dialog.start_failed

                Layout.fillWidth: true
                Layout.topMargin: 4

                Repeater {
                    model: SsdStatus.maintenance_phases

                    delegate: RowLayout {
                        id: step

                        required property string modelData
                        required property int index

                        readonly property int current: dialog.job_started ? SsdStatus.maintenance_phases.indexOf(dialog.job.phase) : -1
                        readonly property string state_kind: index < current ? "safe" : index === current ? "busy" : "off"

                        spacing: 12
                        Layout.fillWidth: true

                        StatusIcon {
                            kind: step.state_kind
                            Layout.preferredWidth: 13
                            Layout.preferredHeight: 13
                        }

                        Label {
                            text: SsdStatus.phase_text(step.modelData)
                            font: AppFont.status_title
                            color: step.state_kind === "off" ? Colour.text_muted : Colour.text

                            Layout.fillWidth: true
                        }

                        Label {
                            visible: step.state_kind === "busy"
                            text: SsdStatus.maintenance_counts(dialog.job)
                            font: AppFont.status_text
                            color: Colour.text_muted
                        }
                    }
                }

                Label {
                    visible: !dialog.job_started || dialog.job.phase === "queued"
                    text: dialog.job_started ? qsTr("Waiting to start…") : qsTr("Sending the request to the device…")
                    font: AppFont.status_text
                    color: Colour.text_muted
                }
            }

            // Results once finished.
            RowLayout {
                visible: !!dialog.job_result
                spacing: 18

                Layout.fillWidth: true

                Label {
                    text: dialog.job && dialog.job.filesystem_repaired ? qsTr("Filesystem repaired and checked") : qsTr("Filesystem not repaired")
                    font: AppFont.status_text
                    color: Colour.text
                }

                Label {
                    text: dialog.job ? qsTr("%1 recovered · %2 failed · %3 backed up").arg(dialog.job.files_recovered || 0).arg(dialog.job.files_failed || 0).arg(dialog.job.files_backed_up || 0) : ""
                    font: AppFont.status_text
                    color: Colour.text_muted
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                visible: !!dialog.job_result
                color: Colour.background_1
                border.color: Colour.border_2
                border.width: 1
                radius: 3

                Layout.fillWidth: true
                Layout.fillHeight: true

                Label {
                    anchors.centerIn: parent
                    visible: !dialog.job || !(dialog.job.files || []).length
                    text: qsTr("No interrupted recordings were found.")
                    font: AppFont.status_text
                    color: Colour.text_muted
                }

                ListView {
                    id: files

                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: dialog.job ? (dialog.job.files || []).length : 0

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Item {
                        id: file_row

                        required property int index
                        readonly property var file: dialog.job.files[index] || ({})
                        readonly property bool recovered: file.status === "recovered"

                        width: files.width
                        height: 28

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: "#3e3e3e"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 16
                            spacing: 10

                            Label {
                                // Last two path parts: camera folder and session file.
                                text: (file_row.file.file || "").split("/").slice(-2).join("/")
                                font: AppFont.status_small
                                color: Colour.text
                                elide: Text.ElideMiddle

                                Layout.preferredWidth: 300
                                Layout.maximumWidth: 300
                            }

                            StatusIcon {
                                kind: file_row.recovered ? "ok" : "caution"
                                Layout.preferredWidth: 10
                                Layout.preferredHeight: 10
                            }

                            Label {
                                text: file_row.recovered ? qsTr("Recovered") : qsTr("Unrecoverable")
                                font: AppFont.status_small
                                color: Colour.text

                                Layout.preferredWidth: 90
                            }

                            Label {
                                id: file_details

                                text: file_row.recovered ? qsTr("%1 frames · %2 s · %3").arg(file_row.file.frames || 0).arg(Number(file_row.file.duration_seconds || 0).toFixed(1)).arg((file_row.file.codec || "").toUpperCase()) : (file_row.file.reason || "")
                                font: AppFont.status_small
                                color: Colour.text_muted
                                elide: Text.ElideRight

                                Layout.fillWidth: true

                                MouseArea {
                                    id: file_mouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }

                                ToolTip.visible: file_mouse.containsMouse && file_details.truncated
                                ToolTip.text: file_details.text
                            }
                        }
                    }
                }
            }

            Label {
                visible: !!dialog.job_result && !!dialog.job && (dialog.job.files_backed_up || 0) > 0
                text: qsTr("Backups of the original files stay on the device (job %1). They aren't deleted automatically.").arg(dialog.job ? dialog.job.id : "")
                textFormat: Text.PlainText
                font: AppFont.status_small
                color: Colour.text_muted
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            Item {
                visible: !dialog.job_result
                Layout.fillHeight: true
            }
        }
    }

    footer_buttons: [
        CompactButton {
            visible: dialog.view === "confirm"
            text: qsTr("Cancel")
            variant: "ghost"
            implicitHeight: 28

            onClicked: dialog.reject()
        },
        CompactButton {
            objectName: "confirmRepairButton"
            visible: dialog.view === "confirm"
            text: qsTr("Pause SSD recording and repair")
            variant: "caution"
            implicitHeight: 28
            enabled: dialog.same_attachment && SsdStatus.repair_enabled && !DeviceRecording.busy && !SsdStatus.maintenance_active

            onClicked: {
                if (!dialog.same_attachment || !SsdStatus.repair_enabled)
                    return;
                dialog.previous_job = SsdStatus.maintenance ? SsdStatus.maintenance.id : "";
                dialog.waiting_for_job = true;
                dialog.view = "job";
                DeviceRecording.storage("repair", true);
            }
        },
        CompactButton {
            objectName: "dismissRepairResultButton"
            visible: dialog.view === "job" && !!dialog.job_result
            text: qsTr("Dismiss result")
            variant: "ghost"
            implicitHeight: 28

            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Stop showing this result as the current issue.")

            onClicked: {
                DeviceRecording.dismissed_job = dialog.job.id;
                dialog.close();
            }
        },
        CompactButton {
            visible: dialog.view === "job"
            text: qsTr("Close")
            implicitHeight: 28

            onClicked: dialog.close()
        }
    ]
}
