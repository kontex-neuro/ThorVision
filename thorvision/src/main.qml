import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

ApplicationWindow {
    id: window
    visible: true
    title: Theme.AppSettings.app_name
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight

    minimumWidth: camera_list.width + record_settings.width + record.width + xdaq_status.width

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            id: top
            Layout.preferredWidth: parent.width
            Layout.fillWidth: true
            spacing: 0

            Layout.topMargin: 1 // to show top border

            CameraList {
                id: camera_list
                Layout.preferredWidth: 540
                Layout.preferredHeight: 110
            }

            Rectangle {
                id: blank
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                border.color: "white"
                border.width: 1
            }

            RecordSettings {
                id: record_settings
                Layout.preferredWidth: 630
                Layout.preferredHeight: 110
            }

            Record {
                id: record
                Layout.preferredWidth: 110
                Layout.preferredHeight: 110
            }

            XDAQStatus {
                id: xdaq_status
                Layout.preferredWidth: 110
                Layout.preferredHeight: 110
            }
        }

        Rectangle {
            color: "transparent"
            border.color: "white"
            border.width: 1
            Layout.fillWidth: true
            Layout.preferredHeight: 15
        }

        VideoLayout {
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.height - top.height
        }
    }
}
