pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// SSD details. Opening or reading it never affects recording. Layout, top to bottom:
// last event (history), current issue (live), storage | cameras, then connection and
// SSD actions. Fits the standard 847x534 AlertDialog.
AlertDialog {
    id: dialog

    objectName: "ssdDetailsDialog"

    signal repair_requested
    signal events_requested

    readonly property var storage: SsdStatus.storage
    readonly property var issue: SsdStatus.current_issue
    readonly property bool can_operate: SsdStatus.connected && !DeviceRecording.busy
    readonly property int segment_seconds: SsdStatus.snapshot.segment_seconds || 60

    // Shown when the user tries to select a fifth camera.
    property string limit_name: ""
    // Edited segment length, or -1 when the field shows the device value.
    property int pending_segment: -1

    title_text: qsTr("SSD details")
    icon_source: "qrc:/qt/qml/App/Theme/resources/ssd.svg"
    footer_left_margin: 24
    footer_right_margin: 24
    footer_bottom_margin: 16

    onClosed: {
        limit_name = "";
        pending_segment = -1;
    }

    component Panel: Rectangle {
        color: Colour.background_1
        border.color: Colour.border_2
        border.width: 1
        radius: 3
    }

    component Caption: Label {
        font: AppFont.status_label
        color: Colour.text_muted
    }

    component LastKnown: Label {
        visible: !SsdStatus.connected
        text: qsTr("Last known")
        font: AppFont.status_label
        color: Colour.text_muted
        leftPadding: 4
        rightPadding: 4

        background: Rectangle {
            color: "transparent"
            border.color: "#6a6a6a"
            border.width: 1
            radius: 2
        }
    }

    content_data: ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 14
        anchors.bottomMargin: 8
        spacing: 9

        // Current issue only: what needs attention now. Past activity lives in the event
        // log (Event log… in the footer), which the device keeps across reboots.
        Rectangle {
            id: status_panel

            readonly property string kind: dialog.issue ? dialog.issue.kind : "ok"

            radius: 3
            border.width: 1
            color: {
                switch (kind) {
                case "caution":
                    return Colour.caution_panel;
                case "critical":
                    return Colour.critical_panel;
                case "busy":
                    return Colour.info_panel;
                }
                return Colour.background_1;
            }
            border.color: {
                switch (kind) {
                case "caution":
                    return Colour.caution_panel_border;
                case "critical":
                    return Colour.critical_panel_border;
                case "busy":
                    return Colour.info_panel_border;
                case "ok":
                    return Colour.border_2;
                }
                return Colour.border_1;
            }

            Layout.fillWidth: true
            Layout.preferredHeight: issue_text.implicitHeight + 16

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                StatusIcon {
                    kind: status_panel.kind

                    Layout.preferredWidth: 13
                    Layout.preferredHeight: 13
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 2
                }

                Label {
                    id: issue_text

                    text: dialog.issue ? qsTr("<b>Current issue:</b> %1").arg(dialog.issue.text) : qsTr("No current issues.")
                    textFormat: Text.StyledText
                    font: AppFont.status_text
                    color: Colour.text
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight

                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            spacing: 12

            Layout.fillWidth: true
            Layout.fillHeight: true

            // Storage
            Panel {
                Layout.preferredWidth: 270
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 7

                    RowLayout {
                        Layout.fillWidth: true

                        Caption {
                            text: qsTr("Storage")
                            Layout.fillWidth: true
                        }

                        StatusIcon {
                            kind: SsdStatus.storage_kind
                            Layout.preferredWidth: 11
                            Layout.preferredHeight: 11
                        }

                        Label {
                            text: SsdStatus.storage_state
                            font: AppFont.status_text
                            color: Colour.text
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Volume ID")
                            font: AppFont.status_small
                            color: Colour.text_muted
                            Layout.fillWidth: true
                        }

                        Label {
                            text: dialog.storage.uuid || qsTr("Unavailable")
                            textFormat: Text.PlainText
                            font: AppFont.status_small
                            color: Colour.text
                        }
                    }

                    Rectangle {
                        readonly property real total: Number(dialog.storage.total_bytes || 0)

                        color: Colour.storage_track
                        border.color: "#555555"
                        border.width: 1
                        radius: 3
                        clip: true
                        opacity: SsdStatus.connected ? 1 : 0.55

                        Layout.fillWidth: true
                        Layout.preferredHeight: 10

                        Rectangle {
                            width: parent.total ? parent.width * (1 - dialog.storage.free_bytes / parent.total) : 0
                            height: parent.height
                            color: Colour.storage_used
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: parent.total ? parent.width * (dialog.storage.reserve_bytes || 0) / parent.total : 0
                            height: parent.height
                            color: Colour.storage_reserved
                        }
                    }

                    GridLayout {
                        columns: 3
                        rowSpacing: 3
                        columnSpacing: 8
                        opacity: SsdStatus.connected ? 1 : 0.55

                        Layout.fillWidth: true

                        Repeater {
                            model: [
                                {
                                    swatch: Colour.storage_used,
                                    label: qsTr("Used (includes other files)"),
                                    value: dialog.storage.total_bytes ? SsdStatus.bytes(dialog.storage.total_bytes - dialog.storage.free_bytes) : "–"
                                },
                                {
                                    swatch: Colour.storage_track,
                                    label: qsTr("Free"),
                                    value: dialog.storage.total_bytes ? SsdStatus.bytes(dialog.storage.free_bytes) : "–"
                                },
                                {
                                    swatch: Colour.storage_reserved,
                                    label: qsTr("Reserved (part of free)"),
                                    value: dialog.storage.total_bytes ? SsdStatus.bytes(dialog.storage.reserve_bytes) : "–"
                                },
                                {
                                    swatch: "transparent",
                                    label: qsTr("Total"),
                                    value: dialog.storage.total_bytes ? SsdStatus.bytes(dialog.storage.total_bytes) : "–"
                                }
                            ]

                            delegate: Item {
                                id: legend_row

                                required property var modelData

                                Layout.columnSpan: 3
                                Layout.fillWidth: true
                                Layout.preferredHeight: 16

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 8

                                    Rectangle {
                                        color: legend_row.modelData.swatch
                                        border.color: legend_row.modelData.swatch === "transparent" ? "transparent" : "#666666"
                                        border.width: 1
                                        radius: 2

                                        Layout.preferredWidth: 10
                                        Layout.preferredHeight: 10
                                    }

                                    Label {
                                        text: legend_row.modelData.label
                                        font: AppFont.status_small
                                        color: Colour.text
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: legend_row.modelData.value
                                        font: AppFont.status_small
                                        color: Colour.text
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 6

                        Layout.fillWidth: true
                        Layout.topMargin: 2

                        Label {
                            text: SsdStatus.retention
                            font: SsdStatus.retention_hours > 0 ? AppFont.status_value : AppFont.status_text
                            color: Colour.text
                        }

                        Label {
                            visible: SsdStatus.retention_hours > 0
                            text: qsTr("est. retention at current rate")
                            font: AppFont.status_small
                            color: Colour.text_muted
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        LastKnown {
                            // Capacity shown without a usable SSD is the last value the device reported.
                            visible: !SsdStatus.connected || (SsdStatus.storage_kind !== "ok" && !!dialog.storage.total_bytes)
                        }
                    }

                    // Estimate wording per the requirements: not footage on disk, not a countdown.
                    Label {
                        text: qsTr("Oldest finished recordings are deleted when space is needed. The estimate is not a countdown.")
                        font: AppFont.status_small
                        color: Colour.text_muted
                        wrapMode: Text.WordWrap
                        lineHeight: 1.15

                        Layout.fillWidth: true
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Cameras
            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 4

                        Caption {
                            text: qsTr("Cameras")
                            Layout.fillWidth: true
                        }

                        Label {
                            text: SsdStatus.summary
                            font: AppFont.status_small
                            color: Colour.text
                        }

                        LastKnown {}
                    }

                    Repeater {
                        // A count, not the array, so rows persist across polls.
                        model: SsdStatus.cameras.length

                        delegate: Item {
                            id: camera_row

                            required property int index
                            readonly property var camera: SsdStatus.cameras[index] || ({})
                            readonly property string kind: SsdStatus.kind_for_camera(camera.state, camera.enabled)

                            Layout.fillWidth: true
                            Layout.preferredHeight: 27

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: "#3e3e3e"
                            }

                            RowLayout {
                                anchors.fill: parent
                                spacing: 8

                                CustomCheckBox {
                                    checked: !!camera_row.camera.enabled
                                    enabled: dialog.can_operate
                                    padding: 0

                                    Accessible.name: qsTr("Record %1 to the SSD").arg(SsdStatus.camera_name(camera_row.camera))

                                    Layout.preferredWidth: 18

                                    onClicked: {
                                        if (checked && SsdStatus.selected_count >= 4) {
                                            dialog.limit_name = SsdStatus.camera_name(camera_row.camera);
                                        } else {
                                            dialog.limit_name = "";
                                            DeviceRecording.set_enabled(camera_row.camera.stable_id, checked);
                                        }
                                        // The device decides; show its answer, not the click.
                                        checked = Qt.binding(() => !!camera_row.camera.enabled);
                                    }
                                }

                                Label {
                                    text: qsTr("%1 · %2").arg(camera_row.camera.slot + 1).arg(SsdStatus.camera_name(camera_row.camera))
                                    font: AppFont.status_small
                                    color: camera_row.camera.enabled ? Colour.text : Colour.text_muted
                                    elide: Text.ElideRight

                                    Layout.preferredWidth: 116
                                }

                                StatusIcon {
                                    kind: camera_row.kind

                                    Layout.preferredWidth: 11
                                    Layout.preferredHeight: 11
                                }

                                Label {
                                    id: state_label

                                    text: {
                                        if (!SsdStatus.connected)
                                            return qsTr("Unknown");
                                        if (!camera_row.camera.enabled)
                                            return qsTr("Not selected");
                                        return camera_row.camera.reason ? qsTr("%1 · %2").arg(camera_row.camera.state).arg(camera_row.camera.reason) : camera_row.camera.state;
                                    }
                                    font: AppFont.status_small
                                    color: camera_row.camera.enabled ? Colour.text : Colour.text_muted
                                    elide: Text.ElideRight

                                    Layout.fillWidth: true

                                    MouseArea {
                                        id: state_mouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }

                                    ToolTip.visible: state_mouse.containsMouse && state_label.truncated
                                    ToolTip.text: state_label.text
                                }

                                Label {
                                    text: SsdStatus.profile_text(camera_row.camera.profile)
                                    font: AppFont.status_small
                                    color: camera_row.camera.enabled ? Colour.text : Colour.text_muted
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: dialog.limit_name.length > 0
                        color: Colour.caution_panel
                        border.color: Colour.caution_panel_border
                        border.width: 1
                        radius: 3

                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        Layout.preferredHeight: limit_text.implicitHeight + 12

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            StatusIcon {
                                kind: "caution"
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                            }

                            Label {
                                id: limit_text

                                text: qsTr("Up to <b>4 cameras</b> can record to the SSD. To record <b>%1</b>, clear another camera first.").arg(dialog.limit_name)
                                textFormat: Text.StyledText
                                font: AppFont.status_small
                                color: Colour.text
                                wrapMode: Text.WordWrap

                                Layout.fillWidth: true
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    // Segment length: impact is explained before Apply; no modal, nothing is lost.
                    Rectangle {
                        color: "#3e3e3e"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        Layout.bottomMargin: 8
                    }

                    RowLayout {
                        spacing: 8

                        Layout.fillWidth: true

                        Caption {
                            text: qsTr("Segment length")
                        }

                        CustomSpinBox {
                            id: segment_box

                            from: 10
                            to: 3600
                            value: dialog.pending_segment >= 0 ? dialog.pending_segment : dialog.segment_seconds
                            enabled: dialog.can_operate

                            Layout.preferredWidth: 62

                            onValueModified: dialog.pending_segment = value
                        }

                        Label {
                            text: dialog.pending_segment >= 0 && dialog.pending_segment !== dialog.segment_seconds ? qsTr("s · changed") : qsTr("s per file, all cameras")
                            font: AppFont.status_small
                            color: dialog.pending_segment >= 0 && dialog.pending_segment !== dialog.segment_seconds ? Colour.status_caution : Colour.text_muted

                            Layout.fillWidth: true
                        }

                        CompactButton {
                            visible: dialog.pending_segment >= 0 && dialog.pending_segment !== dialog.segment_seconds
                            text: qsTr("Apply to all cameras")
                            variant: "primary"
                            implicitHeight: 22
                            enabled: dialog.can_operate

                            onClicked: {
                                DeviceRecording.set_segment_seconds(dialog.pending_segment);
                                dialog.pending_segment = -1;
                                segment_box.value = Qt.binding(() => dialog.pending_segment >= 0 ? dialog.pending_segment : dialog.segment_seconds);
                            }
                        }

                        CompactButton {
                            visible: dialog.pending_segment >= 0 && dialog.pending_segment !== dialog.segment_seconds
                            text: qsTr("Cancel")
                            variant: "ghost"
                            implicitHeight: 22

                            onClicked: {
                                dialog.pending_segment = -1;
                                segment_box.value = Qt.binding(() => dialog.pending_segment >= 0 ? dialog.pending_segment : dialog.segment_seconds);
                            }
                        }
                    }

                    Label {
                        visible: dialog.pending_segment >= 0 && dialog.pending_segment !== dialog.segment_seconds
                        text: qsTr("Applies now to all %1 recording cameras. Current files are finalized and new %2 s files start.").arg(SsdStatus.recording_count).arg(dialog.pending_segment)
                        font: AppFont.status_small
                        color: Colour.status_caution
                        wrapMode: Text.WordWrap

                        Layout.fillWidth: true
                        Layout.topMargin: 4
                    }
                }
            }
        }
    }

    footer_data: RowLayout {
        anchors.fill: parent
        spacing: 8

        StatusIcon {
            kind: SsdStatus.connected ? "ok" : "unknown"

            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
        }

        Label {
            text: {
                const time = SsdStatus.snapshot.observed_at ? new Date(Number(SsdStatus.snapshot.observed_at) * 1000).toLocaleTimeString(Qt.locale(), "HH:mm:ss") : "";
                if (SsdStatus.connected)
                    return time ? qsTr("Connected · updated %1").arg(time) : qsTr("Connected");
                return time ? qsTr("Disconnected · last update %1").arg(time) : qsTr("Disconnected");
            }
            font: AppFont.status_small
            color: Colour.text_muted
        }

        Label {
            visible: !SsdStatus.connected
            text: qsTr("Stale")
            font: AppFont.status_label
            color: Colour.status_caution
            leftPadding: 4
            rightPadding: 4

            background: Rectangle {
                color: "transparent"
                border.color: Colour.caution_panel_border
                border.width: 1
                radius: 2
            }
        }

        Item {
            Layout.fillWidth: true
        }

        CompactButton {
            objectName: "eventLogButton"
            text: qsTr("Event log…")
            variant: "ghost"

            onClicked: dialog.events_requested()
        }

        Item {
            implicitWidth: repair_button.implicitWidth
            implicitHeight: repair_button.implicitHeight

            CompactButton {
                id: repair_button

                anchors.fill: parent
                text: qsTr("Repair filesystem…")
                variant: "ghost"
                enabled: dialog.can_operate && !!dialog.storage.repair_enabled

                onClicked: {
                    dialog.close();
                    dialog.repair_requested();
                }
            }

            HoverHandler {
                id: repair_hover
            }

            ToolTip.visible: repair_hover.hovered && !repair_button.enabled
            ToolTip.text: qsTr("Not available in this version. Repair the SSD on a PC.")
        }

        CompactButton {
            text: SsdStatus.safe_to_remove ? qsTr("Resume recording") : qsTr("Safely eject SSD")
            variant: SsdStatus.safe_to_remove ? "primary" : "caution"
            enabled: dialog.can_operate && SsdStatus.has_volume

            onClicked: DeviceRecording.storage(SsdStatus.safe_to_remove ? "resume" : "eject")
        }

        CompactButton {
            text: qsTr("Close")

            onClicked: dialog.close()
        }
    }
}
