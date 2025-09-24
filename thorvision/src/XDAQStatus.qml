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
                source: "qrc:/qt/qml/App/Theme/resources/xdaq.svg"
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
                    source: "qrc:/qt/qml/App/Theme/resources/xdaq-connected.png"
                    fillMode: Image.PreserveAspectFit
                    anchors.fill: parent
                }
            }

            Item {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 39

                AnimatedImage {
                    source: "qrc:/qt/qml/App/Theme/resources/xdaq-connecting.gif"
                    anchors.fill: parent
                }
            }
        }
    }
}
