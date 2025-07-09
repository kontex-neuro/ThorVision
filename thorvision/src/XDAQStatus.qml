import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: xdaq_status

    property int index: AppSettings.xdaq_connected ? 1 : 0

    StackLayout {
        currentIndex: xdaq_status.index

        Rectangle {
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.preferredWidth: 110
            Layout.preferredHeight: 110

            ColumnLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -9

                Rectangle {
                    color: "transparent"
                    Layout.preferredWidth: 87
                    Layout.preferredHeight: 87

                    Image {
                        source: "qrc:/xdaq.svg"
                        fillMode: Image.PreserveAspectFit
                        anchors.fill: parent
                    }
                }

                Image {
                    source: "qrc:/xdaq-connecting.svg"
                    fillMode: Image.PreserveAspectFit
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignCenter

                    Label {
                        text: qsTr("Connecting...")
                        font: Theme.Font.xdaq_status

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -21
                    }
                }
            }
        }

        Rectangle {
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.preferredWidth: 110
            Layout.preferredHeight: 110

            ColumnLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -9

                Rectangle {
                    color: "transparent"
                    Layout.preferredWidth: 87
                    Layout.preferredHeight: 87

                    Image {
                        source: "qrc:/xdaq.svg"
                        fillMode: Image.PreserveAspectFit
                        anchors.fill: parent
                    }
                }

                Image {
                    source: "qrc:/xdaq-connected.svg"
                    fillMode: Image.PreserveAspectFit
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignCenter

                    Label {
                        text: qsTr("Connected")
                        font: Theme.Font.xdaq_status

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -21
                    }
                }
            }
        }
    }
}
