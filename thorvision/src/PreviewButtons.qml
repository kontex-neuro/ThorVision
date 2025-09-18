import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    ColumnLayout {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 48
        spacing: 26

        Button {
            icon.source: "qrc:/preview-1.svg"
            icon.color: hovered || Theme.AppSettings.selected_preview_index === 1 ? Theme.Color.accent : "transparent"
            icon.width: 23
            icon.height: 23
            padding: 0

            Layout.alignment: Qt.AlignCenter

            background: Rectangle {
                color: "transparent"
            }

            onClicked: {
                Theme.AppSettings.selected_preview_index = 1;
            }
        }

        Button {
            icon.source: "qrc:/preview-4.svg"
            icon.color: hovered || Theme.AppSettings.selected_preview_index === 2 ? Theme.Color.accent : "transparent"
            icon.width: 28
            icon.height: 28
            padding: 0

            Layout.alignment: Qt.AlignCenter

            background: Rectangle {
                color: "transparent"
            }

            onClicked: {
                Theme.AppSettings.selected_preview_index = 2;
            }
        }

        Button {
            icon.source: "qrc:/preview-6.svg"
            icon.color: hovered || Theme.AppSettings.selected_preview_index === 3 ? Theme.Color.accent : "transparent"
            icon.width: 28
            icon.height: 18
            padding: 0

            Layout.alignment: Qt.AlignCenter

            background: Rectangle {
                color: "transparent"
            }

            onClicked: {
                Theme.AppSettings.selected_preview_index = 3;
            }
        }
        Button {
            icon.source: "qrc:/preview-12.svg"
            icon.color: hovered || Theme.AppSettings.selected_preview_index === 4 ? Theme.Color.accent : "transparent"
            icon.width: 28
            icon.height: 20
            padding: 0

            Layout.alignment: Qt.AlignCenter

            background: Rectangle {
                color: "transparent"
            }

            onClicked: {
                Theme.AppSettings.selected_preview_index = 4;
            }
        }
    }
}
