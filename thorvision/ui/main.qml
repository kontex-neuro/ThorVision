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
    minimumWidth: camera_list.width + camera_count.width + record.width + xdaq_status.width
    minimumHeight: 450

    menuBar: Theme.Menu {}

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            spacing: 0

            Layout.topMargin: 1 // to show top border
            Layout.fillWidth: true
            Layout.preferredHeight: 111

            CameraList {
                id: camera_list

                Layout.preferredWidth: 435
                Layout.preferredHeight: parent.height
            }

            CameraCount {
                id: camera_count

                Layout.preferredWidth: 89
                Layout.preferredHeight: parent.height
            }

            Rectangle {
                id: blank
                color: Theme.Color.spacer
                border.color: Theme.Color.spacer_border
                border.width: 1

                Layout.preferredWidth: 306
                Layout.fillWidth: true
                Layout.preferredHeight: parent.height
            }

            RecordSettings {
                id: record_settings

                Layout.preferredWidth: 646
                Layout.preferredHeight: parent.height
                Layout.maximumWidth: 646
                Layout.fillWidth: true
            }

            Record {
                id: record

                Layout.preferredWidth: 110
                Layout.preferredHeight: parent.height
            }

            XDAQStatus {
                id: xdaq_status

                Layout.preferredWidth: 110
                Layout.preferredHeight: parent.height
            }
        }

        Rectangle {
            color: Theme.Color.top_spacer

            Layout.fillWidth: true
            Layout.preferredHeight: 19
        }

        Rectangle {
            color: Theme.Color.top_spacer_border

            Layout.fillWidth: true
            Layout.preferredHeight: 2
        }

        RowLayout {
            spacing: 0

            ColumnLayout {
                spacing: 0

                RowLayout {
                    spacing: 0

                    Layout.fillWidth: true

                    PreviewButtons {
                        Layout.preferredWidth: 78
                        Layout.fillHeight: true
                    }

                    VideoLayout {
                        objectName: "video_layout"

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    StatusDrawer {
                    }
                }

                Theme.StatusBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.AppSettings.api_control ? 37 : 0
                }
            }

            Rectangle {
                color: Theme.Color.camera_settings_border

                Layout.preferredWidth: 2
                Layout.fillHeight: true
            }

            SettingsView {
                Layout.preferredWidth: Theme.AppSettings.camera_settings_visible ? 265 : 0
                Layout.fillHeight: true
            }
        }
    }

    onClosing: close => {
        if (Theme.AppSettings.recording) {
            close.accepted = false;
            close_app_dialog.open();
        }
    }

    AlertDialog {
        id: close_app_dialog

        title_text: qsTr("Warning")
        content_data: Label {
            anchors.top: parent.top
            anchors.topMargin: 197
            anchors.horizontalCenter: parent.horizontalCenter

            text: qsTr("Recording is in progress; you have to stop recording before closing the \napplication.")
            font: Theme.Font.popup_text
            color: Theme.Color.text
            lineHeightMode: Text.FixedHeight
            lineHeight: 30
            horizontalAlignment: Text.AlignHCenter
        }
        footer_data: CustomDialogButton {
            button_text: qsTr("OK")
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            onClicked: {
                close_app_dialog.close();
            }
        }
    }

    property string camera_name: ""

    Connections {
        target: CameraModel
        function onCamera_unplugged_during_recording(name) {
            window.camera_name = name;
            camera_unplugged_dialog.open();
        }
    }

    AlertDialog {
        id: camera_unplugged_dialog

        title_text: qsTr("Camera Connection Lost")
        content_data: Label {
            anchors.top: parent.top
            anchors.topMargin: 212
            anchors.horizontalCenter: parent.horizontalCenter

            text: qsTr("Camera “%1” has been disconnected.").arg(window.camera_name)
            font: Theme.Font.popup_text
            color: Theme.Color.text
            lineHeightMode: Text.FixedHeight
            lineHeight: 30
            horizontalAlignment: Text.AlignHCenter
        }
        footer_data: CustomDialogButton {
            button_text: qsTr("OK")
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            onClicked: {
                camera_unplugged_dialog.close();
            }
        }
    }

    Connections {
        target: Server
        function onStatus_change(connected) {
            if (!connected && Theme.AppSettings.recording) {
                Recorder.stop();
                xdaq_disconnected_dialog.open();
            }
        }
    }

    AlertDialog {
        id: xdaq_disconnected_dialog

        title_text: qsTr("Oops...")
        content_data: Label {
            anchors.top: parent.top
            anchors.topMargin: 197
            anchors.horizontalCenter: parent.horizontalCenter

            text: qsTr("The connection to XDAQ has been lost. Recording will be terminated \nand all settings will be reset to their defaults.")
            font: Theme.Font.popup_text
            color: Theme.Color.text
            lineHeightMode: Text.FixedHeight
            lineHeight: 30
            horizontalAlignment: Text.AlignHCenter
        }
        footer_data: CustomDialogButton {
            button_text: qsTr("Continue")
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            onClicked: {
                xdaq_disconnected_dialog.close();
            }
        }
    }
}
