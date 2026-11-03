import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 
import App.Extensions 1.0

Item {
    id: preview

    property var selected_camera_index: 0
    property bool info_visible: false
    property bool show_analytics: true

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

        // Device-rendered analytics image; shown only while fresh, otherwise the raw video shows.
        ProcessedPreviewItem {
            id: processed_preview
            anchors.fill: parent
            anchors.margins: video_container.border.width
            client: Extensions
            camera: preview.camera ? preview.camera.stable_id : ""
            visible: fresh && preview.show_analytics
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                CameraModel.selected_camera_index = preview.selected_camera_index;
            }
        }
    }

    Row {
        id: view_switch
        anchors.right: info_button.left
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 10
        spacing: 4
        visible: !!preview.camera && Extensions.connected

        Button {
            text: qsTr("Raw")
            checked: !preview.show_analytics
            onClicked: preview.show_analytics = false
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Show live camera video. Analytics and recording continue.")
        }
        Button {
            text: qsTr("Analytics")
            checked: preview.show_analytics
            onClicked: preview.show_analytics = true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Show processed video when available; otherwise show raw video.")
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
        width: Math.max(0, Math.min(270, (view_switch.visible ? view_switch.x : info_button.x) - x - 10))
    }

    // Both recorders, named: SSD recording runs on the device, PC recording in this app.
    Row {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.bottomMargin: 12
        spacing: 8
        visible: !!preview.camera

        RecordingChip {
            readonly property var device_camera: preview.camera ? SsdStatus.camera_for(preview.camera.stable_id) : null
            readonly property bool selected: !!device_camera && !!device_camera.enabled

            recorder: qsTr("SSD")
            kind: !SsdStatus.connected ? "unknown" : SsdStatus.kind_for_camera(device_camera ? device_camera.state : "", selected)
            state_text: {
                if (!SsdStatus.connected)
                    return qsTr("Unknown");
                if (!selected)
                    return qsTr("Off");
                return device_camera.state === "Recording" ? qsTr("REC") : device_camera.state;
            }
        }

        RecordingChip {
            readonly property var extension: {
                Extensions.state;
                return preview.camera ? Extensions.camera(preview.camera.stable_id) : ({});
            }
            readonly property bool running: !!extension.run && extension.run.state === "running"

            visible: Extensions.connected && (running || !!extension.fault)
            recorder: qsTr("AI")
            kind: extension.fault ? "caution" : processed_preview.fresh ? "ok" : "busy"
            state_text: extension.fault ? qsTr("Fault")
                      : !preview.show_analytics ? qsTr("Raw view")
                      : processed_preview.fresh ? qsTr("On") : qsTr("Waiting · raw view")
        }

        RecordingChip {
            readonly property string pc_state: preview.camera ? preview.camera.pc_recording_state : "Stopped"

            recorder: qsTr("PC")
            kind: pc_state === "Recording" ? "ok" : pc_state === "Stopped" ? "off" : pc_state === "Restarting" ? "busy" : "caution"
            state_text: pc_state === "Recording" ? qsTr("REC") : pc_state === "Stopped" ? qsTr("Off") : pc_state
        }
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
