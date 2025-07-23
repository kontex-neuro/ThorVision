import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: preview

    property alias border: video_container.border
    property alias source: video.source
    property alias camera_name: name.text
    property alias quality: quality.text
    property alias format: format.text
    property alias xdaq_time: xdaq_time.text
    property alias ephys_time: ephys_time.text
    property alias do_word: do_word.text

    property int cameraIndex: -1
    property bool info_visible: false

    Rectangle {
        id: video_container

        anchors.fill: parent
        color: Theme.Color.video
        border.color: Theme.Color.video_border
        border.width: 3

        Image {
            id: video
            anchors.fill: parent
            anchors.margins: video_container.border.width
            fillMode: Image.PreserveAspectFit

            cache: false
        }
    }

    Timer {
        interval: 30
        running: true
        repeat: true

        onTriggered: {
            video.source = "image://video/" + preview.cameraIndex + "?" + Date.now();
        }
    }

    Label {
        id: name

        anchors.left: parent.left
        anchors.topMargin: 15
        anchors.top: parent.top
        anchors.leftMargin: 13

        font: Theme.Font.camera_setting_title
        color: Theme.Color.text
    }

    Rectangle {
        id: infoButton
        width: 18
        height: 18
        radius: 12

        color: preview.info_visible ? Theme.Color.accent : infoButtonMouseArea.containsMouse ? Theme.Color.accent : Theme.Color.spacer
        border.color: Theme.Color.text
        border.width: 1

        anchors.top: parent.top
        anchors.topMargin: 15
        anchors.right: parent.right
        anchors.rightMargin: 11

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
        anchors.topMargin: 5
        anchors.rightMargin: 11

        Rectangle {
            anchors.fill: parent
            color: "black"
            radius: 2
            opacity: 0.35
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
                        text: qsTr("Quality - ")
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: quality
                        text: ""
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("Format - ")
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: format
                        text: ""
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("XDAQ Time - ")
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: xdaq_time
                        text: "0000"
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("Ephys Time - ")
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: ephys_time
                        text: "0000"
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                }
                RowLayout {
                    Label {
                        text: qsTr("DO Word - ")
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: do_word
                        text: "0000"
                        font: Theme.Font.camera_setting_info
                        color: Theme.Color.text
                    }
                }
            }
        }
    }
}
