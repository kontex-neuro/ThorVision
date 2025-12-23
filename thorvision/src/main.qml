import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// import QtQuick.Dialogs

import App.Theme 0.1 as Theme

ApplicationWindow {
    id: window
    visible: true
    title: Theme.AppSettings.app_name
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    minimumWidth: camera_list.width + camera_count.width + record.width + xdaq_status.width
    minimumHeight: 450

    // MessageDialog {
    //     id: about_dialog
    //     text: qsTr("ThorVision")
    //     informativeText: qsTr("Version: 1.0.2")
    //     buttons: MessageDialog.Ok
    // }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&Help")
            // Action {
            //     text: qsTr("&About")
            //     onTriggered: {
            //         about_dialog.open();
            //     }
            // }
            Action {
                text: qsTr("&Documentation")
                onTriggered: {
                    Qt.openUrlExternally(Theme.AppSettings.doc);
                }
            }
            Action {
                text: qsTr("&View License")
                onTriggered: {
                    Qt.openUrlExternally(Theme.AppSettings.license);
                }
            }
            Action {
                text: qsTr("&Report Issue")
                onTriggered: {
                    Qt.openUrlExternally(Theme.AppSettings.report_issue);
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            spacing: 0

            Layout.topMargin: 1 // to show top border
            Layout.fillWidth: true
            Layout.preferredHeight: 111

            Rectangle {
                id: camera_list
                color: Theme.Color.camera_list
                border.color: Theme.Color.camera_list_border
                border.width: 1

                Layout.preferredWidth: 435
                Layout.preferredHeight: parent.height

                CameraList {
                    anchors.fill: parent
                }
            }

            Rectangle {
                id: camera_count
                color: Theme.Color.camera_list
                border.color: Theme.Color.camera_list_border
                border.width: 1

                Layout.preferredWidth: 89
                Layout.preferredHeight: parent.height

                CameraCount {
                    anchors.fill: parent
                }
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

            Rectangle {
                id: record_settings
                color: Theme.Color.spacer
                border.color: Theme.Color.spacer_border
                border.width: 1

                Layout.preferredWidth: 646
                Layout.preferredHeight: parent.height
                Layout.maximumWidth: 646
                Layout.fillWidth: true

                RecordSettings {
                    anchors.fill: parent
                }
            }

            Rectangle {
                id: record
                color: Theme.Color.record
                border.color: Theme.Color.record_border
                border.width: 1

                Layout.preferredWidth: 110
                Layout.preferredHeight: parent.height

                Record {
                    anchors.fill: parent
                }
            }

            Rectangle {
                id: xdaq_status
                color: Theme.Color.xdaq_status
                border.color: Theme.Color.xdaq_status_border
                border.width: 1

                Layout.preferredWidth: 110
                Layout.preferredHeight: parent.height

                XDAQStatus {
                    anchors.fill: parent
                }
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
            Layout.fillWidth: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 78
                Layout.fillHeight: true
                color: Theme.Color.preview

                PreviewButtons {
                    anchors.fill: parent
                }
            }

            Rectangle {
                color: Theme.Color.video_layout
                Layout.fillWidth: true
                Layout.fillHeight: true

                VideoLayout {
                    objectName: "video_layout"
                    anchors.fill: parent
                }
            }

            Rectangle {
                color: Theme.Color.camera_settings_border
                Layout.preferredWidth: 2
                Layout.fillHeight: true
            }

            Rectangle {
                color: Theme.Color.camera_settings
                Layout.preferredWidth: Theme.AppSettings.camera_settings_visible ? 265 : 0
                Layout.fillHeight: true

                SettingsView {
                    anchors.fill: parent
                }
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
