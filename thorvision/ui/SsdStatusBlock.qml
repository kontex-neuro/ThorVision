pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// Top-row SSD block. SSD availability (status line) and camera activity (markers +
// summary) are separate lines on purpose: a Ready SSD does not mean every camera is
// recording. The bottom line shows the one current issue, or stays empty.
//
// Background polls update values in place; only a running operation disables anything.
Rectangle {
    id: block

    color: Colour.background_1
    border.color: Colour.border_1
    border.width: 1

    // Narrow windows drop the storage column; it stays in SSD details.
    readonly property bool compact: width < 480
    readonly property var issue: SsdStatus.current_issue
    readonly property var storage: SsdStatus.storage
    readonly property real used_fraction: storage.total_bytes ? 1 - storage.free_bytes / storage.total_bytes : 0
    readonly property real reserve_fraction: storage.total_bytes ? (storage.reserve_bytes || 0) / storage.total_bytes : 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        // SSD status line
        RowLayout {
            spacing: 8

            Layout.fillWidth: true
            Layout.preferredHeight: 22
            Layout.topMargin: 7
            Layout.leftMargin: 14
            Layout.rightMargin: 10

            Label {
                text: qsTr("SSD")
                font: AppFont.status_text
                color: Colour.text_muted
            }

            StatusIcon {
                kind: SsdStatus.has_status || !SsdStatus.connected ? SsdStatus.storage_kind : "busy"

                Layout.preferredWidth: 12
                Layout.preferredHeight: 12
            }

            Label {
                text: SsdStatus.has_status || !SsdStatus.connected ? SsdStatus.storage_state : qsTr("Loading device status…")
                font: AppFont.status_title
                color: Colour.text
                elide: Text.ElideRight

                Layout.fillWidth: true
            }

            CompactButton {
                objectName: "ssdDetailsButton"
                text: qsTr("SSD details…")
                variant: "ghost"
                implicitHeight: 22

                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("View SSD and recording status. Recording continues.")

                onClicked: ssd_details.open()
            }
        }

        // Cameras | retention, then summary | storage bar
        GridLayout {
            columns: block.compact ? 1 : 2
            rowSpacing: 6
            columnSpacing: 18

            Layout.fillWidth: true
            Layout.topMargin: 7
            Layout.leftMargin: 14
            Layout.rightMargin: 12

            Row {
                spacing: 5

                Layout.preferredHeight: 20
                Layout.fillWidth: true

                Repeater {
                    // A count, not the array: polls replace the array every second, and an
                    // array model would rebuild every marker each time.
                    model: SsdStatus.selected_count

                    delegate: Rectangle {
                        id: marker

                        required property int index
                        readonly property var modelData: SsdStatus.cameras.filter(c => c.enabled)[index] || ({})
                        readonly property string kind: SsdStatus.kind_for_camera(modelData.state, true)

                        width: marker_row.implicitWidth + 12
                        height: 20
                        radius: 3
                        color: Colour.storage_track
                        border.color: "#5a5a5a"
                        border.width: 1

                        Row {
                            id: marker_row

                            anchors.centerIn: parent
                            spacing: 5

                            StatusIcon {
                                kind: marker.kind
                                width: 11
                                height: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                text: marker.modelData.slot + 1
                                font: AppFont.status_small
                                color: Colour.text
                            }
                        }

                        MouseArea {
                            id: marker_mouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        ToolTip.visible: marker_mouse.containsMouse
                        ToolTip.text: qsTr("%1: %2").arg(SsdStatus.camera_name(modelData)).arg(SsdStatus.connected ? (modelData.reason ? modelData.state + " · " + modelData.reason : modelData.state) : qsTr("Unknown"))
                    }
                }
            }

            RowLayout {
                visible: !block.compact
                spacing: 8

                Layout.preferredHeight: 20
                Layout.fillWidth: true

                Label {
                    text: SsdStatus.retention
                    font: SsdStatus.retention_hours > 0 ? AppFont.status_value : AppFont.status_text
                    color: Colour.text
                    opacity: SsdStatus.connected ? 1 : 0.55
                }

                Label {
                    text: SsdStatus.connected ? qsTr("retention") : qsTr("LAST KNOWN")
                    font: SsdStatus.connected ? AppFont.status_small : AppFont.status_label
                    color: Colour.text_muted
                    visible: SsdStatus.retention_hours > 0 || !SsdStatus.connected
                }

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: block.storage.total_bytes ? qsTr("%1 free").arg(SsdStatus.bytes(block.storage.free_bytes)) : ""
                    font: AppFont.status_small
                    color: Colour.text_muted
                }
            }

            Label {
                text: SsdStatus.summary
                font: AppFont.status_text
                color: Colour.text
                elide: Text.ElideRight

                Layout.fillWidth: true
            }

            // Used | free (unreserved) | reserved
            Rectangle {
                visible: !block.compact
                color: Colour.storage_track
                border.color: "#444444"
                border.width: 1
                radius: 3
                clip: true
                opacity: SsdStatus.connected ? 1 : 0.55

                Layout.fillWidth: true
                Layout.preferredHeight: 6

                Rectangle {
                    width: parent.width * block.used_fraction
                    height: parent.height
                    color: Colour.storage_used
                }

                Rectangle {
                    anchors.right: parent.right
                    width: parent.width * block.reserve_fraction
                    height: parent.height
                    color: Colour.storage_reserved
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        // Current issue
        Rectangle {
            color: Colour.background_3
            visible: block.issue !== null

            Layout.fillWidth: true
            Layout.preferredHeight: 26

            Rectangle {
                width: parent.width
                height: 1
                color: Colour.border_2
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 8

                StatusIcon {
                    kind: block.issue ? block.issue.kind : "off"

                    Layout.preferredWidth: 12
                    Layout.preferredHeight: 12
                }

                Label {
                    id: issue_label

                    text: block.issue ? block.issue.text : ""
                    font: AppFont.status_small
                    color: Colour.text
                    elide: Text.ElideRight

                    Layout.fillWidth: true

                    MouseArea {
                        id: issue_mouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    ToolTip.visible: issue_mouse.containsMouse && issue_label.truncated
                    ToolTip.text: issue_label.text
                }
            }
        }
    }

    SsdDetailsDialog {
        id: ssd_details

        onRepair_requested: repair.open()
    }

    AlertDialog {
        id: repair

        title_text: qsTr("Repair recording SSD?")
        content_data: Label {
            anchors.top: parent.top
            anchors.topMargin: 60
            anchors.horizontalCenter: parent.horizontalCenter

            width: parent.width - 150
            text: qsTr("SSD %1\n\nSSD recording will pause. Filesystem repair can change or remove damaged files, so copy important footage first. PC recording can continue while its camera stream is available.").arg(block.storage.uuid || "")
            font: AppFont.popup_scroll_text
            color: Colour.text
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
        }
        footer_buttons: [
            CompactButton {
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: repair.reject()
            },
            CompactButton {
                text: qsTr("Pause SSD recording and repair")
                variant: "caution"
                enabled: !!block.storage.repair_enabled && !DeviceRecording.busy && SsdStatus.connected
                onClicked: {
                    DeviceRecording.storage("repair", true);
                    repair.close();
                }
            }
        ]
    }

    // Swallow clicks so they don't reach items underneath, like the other top-row blocks.
    MouseArea {
        anchors.fill: parent
        z: -1
    }
}
