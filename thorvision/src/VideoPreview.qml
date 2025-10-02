import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

import org.freedesktop.gstreamer.Qt6GLVideoItem 1.0

Item {
    id: preview

    property alias border: video_container.border
    
    property var camera: null
    property int camera_id: -1
    property bool info_visible: false

    property string camera_name: camera ? camera.name : ""
    property string quality: camera ? camera.cap : ""
    property string format: camera ? camera.codec : ""
    property string xdaq_time: camera ? camera.xdaq_timestamp : ""
    property string ephys_time: camera ? camera.rhythm_timestamp : ""
    property string do_word: camera ? camera.ttl_out : ""

    Rectangle {
        id: video_container

        anchors.fill: parent
        color: Theme.Color.video
        border.color: Theme.Color.video_border
        border.width: 3

        GstGLQt6VideoItem {
            id: video
            objectName: "video_item"
            anchors.centerIn: parent
            anchors.margins: video_container.border.width
            width: parent.width
            height: parent.height
        }
    }

    Label {
        id: name

        anchors.left: parent.left
        anchors.topMargin: 15
        anchors.top: parent.top
        anchors.leftMargin: 13

        font: Theme.Font.camera_settings_name
        color: Theme.Color.text
        text: preview.camera_name
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

                RowLayout {
                    spacing: 0

                    Label {
                        text: qsTr("Quality - ")
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: quality
                        text: preview.quality
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                }

                RowLayout {
                    spacing: 0

                    Label {
                        text: qsTr("Format - ")
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: format
                        text: preview.format
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                }

                RowLayout {
                    spacing: 0

                    Label {
                        text: qsTr("XDAQ Time - ")
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: xdaq_time
                        text: preview.xdaq_time
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                }
                RowLayout {
                    spacing: 0

                    Label {
                        text: qsTr("Ephys Time - ")
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: ephys_time
                        text: preview.ephys_time
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                }
                RowLayout {
                    spacing: 0

                    Label {
                        text: qsTr("DO Word - ")
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                    Label {
                        id: do_word
                        text: preview.do_word
                        font: Theme.Font.camera_settings_info
                        color: Theme.Color.text
                    }
                }
            }
        }
    }
}
