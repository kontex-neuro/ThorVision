import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: preview

    property int i: 0
    property alias camera_name: name.text

    property alias quality: quality.text
    property alias format: format.text
    property alias xdaq_time: xdaq_time.text
    property alias ephys_time: ephys_time.text
    property alias do_word: do_word.text

    property bool info_visible: false

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "white"
        border.width: 1

        Image {
            id: video
            anchors.fill: parent
            anchors.margins: 1

            source: "image://video/live"
            fillMode: Image.PreserveAspectFit

            cache: false
        }
    }

    Timer {
        interval: 30
        running: true
        repeat: true
        onTriggered: video.source = "image://video/live?" + Date.now()
    }

    Label {
        id: name
        text: "Camera"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8

        font: Theme.Font.camera_setting_title
    }

    Rectangle {
        id: infoButton
        width: 18
        height: 18
        radius: 12
        color: preview.info_visible ? Theme.Color.accent : infoButtonMouseArea.containsMouse ? Theme.Color.accent : "gray"

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        border.color: "white"
        border.width: 1

        Label {
            text: "i"
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 12
        }

        MouseArea {
            id: infoButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: preview.info_visible = !preview.info_visible
        }
    }

    Item {
        id: infoPanel
        visible: preview.info_visible

        width: camera_info.width + 16
        height: camera_info.height + 16

        anchors.top: infoButton.bottom
        anchors.right: parent.right
        anchors.topMargin: 9
        anchors.rightMargin: 9

        Rectangle {
            anchors.fill: parent
            color: "black"
            radius: 3
            opacity: 0.5
        }

        Item {
            anchors.fill: parent
            anchors.margins: 8

            ColumnLayout {
                id: camera_info
                // Layout.fillWidth: true
                // Layout.fillHeight: true

                RowLayout {
                    Label {
                        text: qsTr("Quality")
                        font: Theme.Font.camera_setting_info
                    }
                    Label {
                        id: quality
                        text: ""
                        font: Theme.Font.camera_setting_info
                        // wrapMode: Text.Wrap
                        // lineHeight: 1.8
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("Format")
                        font: Theme.Font.camera_setting_info
                    }
                    Label {
                        id: format
                        text: ""
                        font: Theme.Font.camera_setting_info
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("XDAQ Time")
                        font: Theme.Font.camera_setting_info
                    }
                    Label {
                        id: xdaq_time
                        text: "0000"
                        font: Theme.Font.camera_setting_info
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("Ephys Time")
                        font: Theme.Font.camera_setting_info
                    }
                    Label {
                        id: ephys_time
                        text: "0000"
                        font: Theme.Font.camera_setting_info
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("DO Word")
                        font: Theme.Font.camera_setting_info
                    }
                    Label {
                        id: do_word
                        text: "0000"
                        font: Theme.Font.camera_setting_info
                    }
                }
            }
        }
    }
}
