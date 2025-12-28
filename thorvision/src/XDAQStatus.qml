import QtQuick
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    ColumnLayout {
        spacing: 18

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/xdaq.svg"
            sourceSize.width: 84
            sourceSize.height: 34
            fillMode: Image.PreserveAspectFit

            Layout.alignment: Qt.AlignCenter
            Layout.topMargin: 11
        }

        StackLayout {
            currentIndex: Theme.AppSettings.xdaq_connected ? 0 : 1

            Image {
                source: "qrc:/qt/qml/App/Theme/resources/xdaq-connected.png"
                sourceSize.width: 110
                sourceSize.height: 39
                fillMode: Image.PreserveAspectFit
            }

            AnimatedImage {
                source: "qrc:/qt/qml/App/Theme/resources/xdaq-connecting.gif"
                sourceSize.width: 110
                sourceSize.height: 39
                cache: false
            }
        }
    }

    // Added to prevent mouse events from underlying items
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
    }
}
