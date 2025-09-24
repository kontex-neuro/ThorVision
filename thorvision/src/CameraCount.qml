import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: window

    StackLayout {
        anchors.fill: parent
        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

        AnimatedImage {
            source: "qrc:/qt/qml/App/Theme/resources/camera-connect.gif"
        }

        Item {
            Image {
                source: "qrc:/qt/qml/App/Theme/resources/camera-connected.svg"
                fillMode: Image.PreserveAspectFit
                anchors.centerIn: parent
            }

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
