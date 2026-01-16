import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Rectangle {
    color: Theme.Color.camera_list
    border.color: Theme.Color.camera_list_border
    border.width: 1

    StackLayout {
        anchors.fill: parent
        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

        AnimatedImage {
            source: "qrc:/qt/qml/App/Theme/resources/camera-connect.gif"
            cache: false
        }

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/camera-connected.svg"
            fillMode: Image.PreserveAspectFit

            Layout.fillWidth: true
            Layout.fillHeight: true

            Label {
                text: qsTr("%1").arg(CameraModel.rowCount)
                font: Theme.Font.camera_count
                color: Theme.Color.text
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -25
            }
        }
    }
}
