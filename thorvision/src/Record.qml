import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Basic

import App.Theme 0.1 as Theme

Item {
    id: record

    property int index: Theme.AppSettings.recording ? 1 : 0

    Rectangle {

        StackLayout {
            currentIndex: record.index

            Rectangle {
                color: "transparent"
                border.color: "white"
                border.width: 1

                Layout.preferredWidth: 110
                Layout.preferredHeight: 110
                // enabled: AppSettings.camera_detected

                Image {
                    source: "qrc:/start-record.svg"
                    fillMode: Image.PreserveAspectFit

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -5

                    // enabled: AppSettings.camera_detected

                    Label {
                        text: qsTr("REC")
                        font: Theme.Font.record
                        // enabled: AppSettings.camera_detected
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 35
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: Theme.AppSettings.camera_detected

                    onClicked: {
                        console.log("Record");
                        AppSettings.recording = !Theme.AppSettings.recording;
                    }

                    onEntered: {
                        parent.opacity = 0.5;
                    }

                    onExited: {
                        parent.opacity = 1;
                    }
                }
            }

            Rectangle {
                color: "transparent"
                border.color: "white"
                border.width: 1

                Layout.preferredWidth: 110
                Layout.preferredHeight: 110
                // enabled: AppSettings.camera_detected

                Image {
                    source: "qrc:/stop-record.svg"
                    fillMode: Image.PreserveAspectFit

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -5

                    // enabled: AppSettings.camera_detected

                    Label {
                        text: qsTr("00:00:00")
                        font: Theme.Font.record
                        // enabled: AppSettings.camera_detected
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 35
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: Theme.AppSettings.camera_detected

                    onClicked: {
                        console.log("Record");
                        AppSettings.recording = !Theme.AppSettings.recording;
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
}
