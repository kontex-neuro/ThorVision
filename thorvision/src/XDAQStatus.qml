import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    ColumnLayout {
        spacing: 18

        Item {
            Layout.preferredWidth: 84
            Layout.preferredHeight: 34
            Layout.alignment: Qt.AlignCenter
            Layout.topMargin: 11

            Image {
                source: "qrc:/xdaq.svg"
                fillMode: Image.PreserveAspectFit
                anchors.fill: parent
            }
        }

        StackLayout {
            currentIndex: Theme.AppSettings.xdaq_connected ? 0 : 1

            Item {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 39

                Image {
                    source: "qrc:/xdaq-connected.png"
                    fillMode: Image.PreserveAspectFit
                    anchors.fill: parent
                }
            }

            Item {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 39

                AnimatedImage {
                    source: "qrc:/xdaq-connecting.gif"
                    anchors.fill: parent
                }
            }
        }
    }
}
