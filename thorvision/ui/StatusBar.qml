import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
    width: parent.width
    height: parent.height
    // edge: Qt.BottomEdge
    visible: Recorder.api_control
    // modal: false
    // interactive: false
    color: Color.status_bar_background

    // background: Rectangle {
    //     color: Color.status_bar_background
    //     anchors.fill: parent
    // }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter

        Label {
            text: qsTr("External API Control:")
            font: AppFont.camera_settings_name
            color: Color.text
        }

        Label {
            id: status_label
            text: qsTr("Active")
            font: AppFont.camera_settings_name
            color: Color.text

            Layout.fillWidth: true
        }

        Label {
            text: Recorder.api_controller_name
            font: AppFont.camera_settings_name
            color: Color.text
        }
    }
}
