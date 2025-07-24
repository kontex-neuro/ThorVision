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

    minimumWidth: camera_list.width + camera_count.width + record_settings.width + record.width + xdaq_status.width

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
                    id: list
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
                    camera_count: list.camera_count
                }
            }

            Rectangle {
                id: blank
                color: Theme.Color.spacer
                border.color: Theme.Color.spacer_border
                border.width: 1

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

                RecordSettings {
                    anchors.fill: parent
                }
            }

            Rectangle {
                id: record
                color: Theme.Color.record
                border.color: Theme.Color.record_border
                border.width: 1
                enabled: Theme.AppSettings.camera_detected

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
                Layout.preferredWidth: 265
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
            dialog.open();
        }
    }

    AlertDialog {
        id: dialog

        title_text: qsTr("Warning")
        content_data: Label {
            anchors.top: parent.top
            anchors.topMargin: 197
            anchors.left: parent.left
            anchors.leftMargin: 88
            anchors.right: parent.right
            anchors.rightMargin: 88

            text: qsTr("Recording is in progress; you have to stop recording before closing the \napplication.")
            font: Theme.Font.popup_normal_text
            color: Theme.Color.text
            lineHeight: 1.5
            horizontalAlignment: Text.AlignHCenter
        }
        footer_data: CustomButton {
            button_text: qsTr("OK")
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            onClicked: {
                dialog.close();
            }
        }
    }
}
