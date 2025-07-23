import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: xdaq_status

    property int index: Theme.AppSettings.xdaq_connected ? 1 : 0

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -9

        Item {
            Layout.preferredWidth: 87
            Layout.preferredHeight: 87

            Image {
                source: "qrc:/xdaq.svg"
                fillMode: Image.PreserveAspectFit
                anchors.fill: parent
            }
        }

        Item {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            Layout.alignment: Qt.AlignCenter

            Image {
                source: Theme.AppSettings.xdaq_connected ? "qrc:/xdaq-connected.svg" : "qrc:/xdaq-connecting.svg"
                fillMode: Image.PreserveAspectFit
                anchors.fill: parent

                Label {
                    text: Theme.AppSettings.xdaq_connected ? qsTr("Connected") : qsTr("Connecting...")
                    font: Theme.Font.xdaq_status
                    color: Theme.Color.text

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -21
                }
            }
        }
    }
}
