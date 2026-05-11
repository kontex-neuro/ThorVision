import QtQuick
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
    color: Colour.xdaq_status
    border.color: Colour.xdaq_status_border
    border.width: 1

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
            currentIndex: AppSettings.xdaq_connected ? 0 : 1

            Image {
                source: "qrc:/qt/qml/App/Theme/resources/xdaq-connected.png"
                fillMode: Image.PreserveAspectFit

                Layout.preferredWidth: 110
                Layout.preferredHeight: 39
            }

            AnimatedImage {
                source: "qrc:/qt/qml/App/Theme/resources/xdaq-connecting.gif"
                cache: false

                Layout.preferredWidth: 110
                Layout.preferredHeight: 39
            }
        }
    }

    // Added to prevent mouse events from underlying items
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
    }
}
