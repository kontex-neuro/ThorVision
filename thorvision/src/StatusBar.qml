import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Rectangle {
    width: parent.width
    height: parent.height
    // edge: Qt.BottomEdge
    visible: Theme.AppSettings.api_control
    // modal: false
    // interactive: false
    color: Theme.Color.status_bar_background

    // background: Rectangle {
    //     color: Theme.Color.status_bar_background
    //     anchors.fill: parent
    // }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter

        Label {
            text: qsTr("External API Control:")
            font: Theme.Font.camera_settings_name
            color: Theme.Color.text
        }

        Label {
            id: status_label
            text: qsTr("Active")
            font: Theme.Font.camera_settings_name
            color: Theme.Color.text

            Layout.fillWidth: true
        }

        Label {
            text: Theme.AppSettings.api_controller_name
            font: Theme.Font.camera_settings_name
            color: Theme.Color.text
        }
    }
}
