import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Item {
    id: preview

    property var selected_camera_index: 0
    property bool info_visible: false

    property var camera: CameraModel.get(selected_camera_index).camera_item
    property string camera_name: camera ? camera.name : ""
    property string quality: camera ? camera.cap : ""
    property string codec: camera ? camera.codec : ""
    property string xdaq_time: camera ? camera.xdaq_timestamp : ""

    Rectangle {
        id: video_container

        anchors.fill: parent
        color: Colour.video
        border.color: (preview.selected_camera_index === CameraModel.selected_camera_index) ? Colour.accent : Colour.video_border
        border.width: 3

        Loader {
            objectName: "loader"
            anchors.fill: parent
            anchors.margins: video_container.border.width
            source: Qt.platform.os === "windows" ? "D3D11GstVideoItem.qml" : "GLGstVideoItem.qml"
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                CameraModel.selected_camera_index = preview.selected_camera_index;
            }
        }
    }

    Label {
        id: name

        anchors.left: parent.left
        anchors.topMargin: 15
        anchors.top: parent.top
        anchors.leftMargin: 13

        font: AppFont.camera_settings_name
        color: Colour.text
        text: preview.camera_name
        elide: Text.ElideRight
        maximumLineCount: 1
        width: 270
    }

    Rectangle {
        id: info_button

        width: 18
        height: 18
        radius: 12

        color: preview.info_visible ? Colour.accent : info_button_mousearea.containsMouse ? Colour.accent : Colour.spacer
        border.color: Colour.text
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
            id: info_button_mousearea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                preview.info_visible = !preview.info_visible;
                forceActiveFocus(Qt.MouseFocusReason);
            }
        }
    }

    Item {
        id: infoPanel
        visible: preview.info_visible

        width: camera_info.width + 16
        height: camera_info.height + 16

        anchors.top: info_button.bottom
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
                        font: AppFont.camera_settings_info
                        color: Colour.text
                    }
                    Label {
                        id: quality
                        text: preview.quality
                        font: AppFont.camera_settings_info
                        color: Colour.text
                    }
                }

                RowLayout {
                    spacing: 0

                    Label {
                        text: qsTr("Codec - ")
                        font: AppFont.camera_settings_info
                        color: Colour.text
                    }
                    Label {
                        id: codec
                        text: preview.codec
                        font: AppFont.camera_settings_info
                        color: Colour.text
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
                        font: AppFont.camera_settings_info
                        color: Colour.text
                    }
                    Label {
                        id: xdaq_time
                        text: preview.xdaq_time
                        font: AppFont.camera_settings_info
                        color: Colour.text
                    }
                }
            }
        }
    }
}
