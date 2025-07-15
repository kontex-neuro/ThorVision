import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: window

    required property int camera_count

    StackLayout {
        anchors.fill: parent
        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

        Image {
            source: "qrc:/camera-connect.svg"
            fillMode: Image.PreserveAspectFit
        }

        Item {
            Image {
                source: "qrc:/camera-connected.svg"
                fillMode: Image.PreserveAspectFit
                anchors.centerIn: parent
            }

            Label {
                text: qsTr("%1").arg(window.camera_count)
                // text: qsTr("%1").arg(CameraModel.count)
                font: Theme.Font.camera_count
                color: Theme.Color.text_1
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -25
            }
        }
    }
}
