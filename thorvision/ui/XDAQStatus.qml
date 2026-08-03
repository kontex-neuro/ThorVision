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

    // Standing reminder that an update was offered and set aside. Not a nag -- the dialog
    // prompts at most once per session -- but when an ignored version mismatch causes odd
    // behaviour later, there needs to be a visible reason on screen.
    UpdateBadge {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 6
        anchors.topMargin: 6
    }

    // Added to prevent mouse events from underlying items
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
    }
}
