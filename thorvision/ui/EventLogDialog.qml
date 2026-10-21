pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// Device recording history: the latest events the device keeps (up to 200, across
// reboots, including while ThorVision is closed). Device-wide, not per SSD: events carry
// no SSD identity. Read-only; opening it never affects recording.
AlertDialog {
    id: dialog

    objectName: "eventLogDialog"

    readonly property var events: SsdStatus.events

    title_text: qsTr("Event log")
    icon_source: "qrc:/qt/qml/App/Theme/resources/ssd.svg"
    footer_left_margin: 24
    footer_right_margin: 24
    footer_bottom_margin: 16

    content_data: ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 14
        anchors.bottomMargin: 8
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Latest %1 device recording events, newest first. Covers every SSD used with this device; there is no fixed time window.").arg(dialog.events.length)
                font: AppFont.status_small
                color: Colour.text_muted
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            Label {
                visible: !SsdStatus.connected
                text: qsTr("Last received")
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
        }

        Rectangle {
            color: Colour.background_1
            border.color: Colour.border_2
            border.width: 1
            radius: 3

            Layout.fillWidth: true
            Layout.fillHeight: true

            Label {
                anchors.centerIn: parent
                visible: dialog.events.length === 0
                text: qsTr("No recording events yet.")
                font: AppFont.status_text
                color: Colour.text_muted
            }

            ListView {
                id: list

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                // A count, not the array: polls replace the array every second.
                model: dialog.events.length

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                header: Item {
                    width: list.width
                    height: 26

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 16
                        spacing: 10

                        Repeater {
                            model: [
                                { text: qsTr("Time"), width: 150 },
                                { text: qsTr("Camera"), width: 150 },
                                { text: qsTr("Event"), width: 175 },
                                { text: qsTr("Details"), width: -1 }
                            ]

                            delegate: Label {
                                required property var modelData

                                text: modelData.text
                                font: AppFont.status_label
                                color: Colour.text_muted

                                Layout.preferredWidth: modelData.width > 0 ? modelData.width : -1
                                Layout.fillWidth: modelData.width < 0
                            }
                        }
                    }
                }

                delegate: Item {
                    id: row

                    required property int index
                    // Newest first; the device sends oldest first.
                    readonly property var event: dialog.events[dialog.events.length - 1 - index] || ({})

                    width: list.width
                    height: 28

                    Rectangle {
                        anchors.top: parent.top
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
                            text: SsdStatus.time_text(row.event.at)
                            font: AppFont.status_small
                            color: Colour.text_muted

                            Layout.preferredWidth: 150
                        }

                        Label {
                            text: SsdStatus.event_camera(row.event) || qsTr("Device")
                            font: AppFont.status_small
                            color: row.event.camera ? Colour.text : Colour.text_muted
                            elide: Text.ElideMiddle

                            Layout.preferredWidth: 150
                        }

                        RowLayout {
                            spacing: 6

                            // Fixed: a nested layout with a fill child would otherwise grow and squeeze Details.
                            Layout.preferredWidth: 175
                            Layout.maximumWidth: 175
                            Layout.fillWidth: false

                            StatusIcon {
                                kind: SsdStatus.kind_for_event(row.event.kind)

                                Layout.preferredWidth: 10
                                Layout.preferredHeight: 10
                            }

                            Label {
                                text: row.event.kind || qsTr("Unknown event")
                                font: AppFont.status_small
                                color: Colour.text
                                elide: Text.ElideRight

                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            id: details_label

                            horizontalAlignment: Text.AlignLeft

                            text: row.event.reason || "–"
                            font: AppFont.status_small
                            color: row.event.reason ? Colour.text : Colour.text_muted
                            elide: Text.ElideRight

                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                            MouseArea {
                                id: details_mouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }

                            ToolTip.visible: details_mouse.containsMouse && details_label.truncated
                            ToolTip.text: details_label.text
                        }
                    }
                }
            }
        }
    }

    footer_buttons: [
        CompactButton {
            text: qsTr("Close")

            onClicked: dialog.close()
        }
    ]
}
