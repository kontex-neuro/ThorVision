import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import App.Theme 0.1 as Theme

Item {
    id: record

    property int index: Theme.AppSettings.recording ? 1 : 0

    StackLayout {
        currentIndex: record.index
        anchors.fill: parent

        Item {
            Image {
                source: "qrc:/start-record.svg"
                fillMode: Image.PreserveAspectFit

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -5

                opacity: Theme.AppSettings.camera_detected ? 1 : 0.5
            }

            Label {
                text: qsTr("REC")
                font: Theme.Font.record

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 30

                opacity: Theme.AppSettings.camera_detected ? 1 : 0.5
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: Theme.AppSettings.camera_detected

                onClicked: {
                    console.log("Record");
                    Theme.AppSettings.recording = !Theme.AppSettings.recording;
                }

                onEntered: {
                    parent.opacity = 0.5;
                }

                onExited: {
                    parent.opacity = 1;
                }
            }
        }

        Item {
            Image {
                source: "qrc:/stop-record.svg"
                fillMode: Image.PreserveAspectFit

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -5
            }

            Label {
                text: qsTr("00:00:00")
                font: Theme.Font.record

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 30
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: Theme.AppSettings.camera_detected

                onClicked: {
                    console.log("Record");
                    Theme.AppSettings.recording = !Theme.AppSettings.recording;
                }

                onEntered: {
                    parent.opacity = 0.5;
                }

                onExited: {
                    parent.opacity = 1;
                }
            }
        }
    }
}
