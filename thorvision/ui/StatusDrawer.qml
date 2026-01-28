import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Drawer {
    id: status
    edge: Qt.LeftEdge
    width: 314
    height: 65
    visible: false
    modal: false
    interactive: false
    y: parent.height - height - 77

    property alias text: status_label.text

    // Added to prevent mouse events from underlying items
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
    }

    Timer {
        id: auto_close_timer
        interval: 3000
        repeat: false
        onTriggered: status.close()
    }

    Connections {
        target: Bus
        function onStatus_notify(message) {
            status.text = message;
            status.open();
            auto_close_timer.restart();
        }
    }

    background: Rectangle {
        color: Theme.Color.background_2
        anchors.fill: parent
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 18

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/checked.svg"
            sourceSize.width: 21
            sourceSize.height: 21
            // mipmap: true
            fillMode: Image.PreserveAspectFit

            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            id: status_label
            font: Theme.Font.camera_settings_name
            color: Theme.Color.text

            Layout.alignment: Qt.AlignVCenter
        }
    }
}
