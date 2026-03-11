import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

ApplicationWindow {
    id: window
    visible: true
    title: AppSettings.app_name
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    minimumWidth: camera_list.width + camera_count.width + record.width + xdaq_status.width
    minimumHeight: 450

    menuBar: Menu {}

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
                color: Color.spacer
                border.color: Color.spacer_border
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
            color: Color.top_spacer

            Layout.fillWidth: true
            Layout.preferredHeight: 19
        }

        Rectangle {
            color: Color.top_spacer_border

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

                    StatusDrawer {}
                }

                StatusBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Recorder.api_control ? 37 : 0
                }
            }

            Rectangle {
                color: Color.camera_settings_border

                Layout.preferredWidth: 2
                Layout.fillHeight: true
            }

            SettingsView {
                Layout.preferredWidth: AppSettings.camera_settings_visible ? 265 : 0
                Layout.fillHeight: true
            }
        }
    }

    AlertDialog {
        id: dialog

        property string camera_name: ""

        title_text: qsTr("Warning")
        content_data: Label {
            id: content_data
            anchors.top: parent.top
            anchors.topMargin: 197
            anchors.horizontalCenter: parent.horizontalCenter

            width: parent.width - 150
            font: AppFont.popup_text
            color: Color.text
            wrapMode: Label.WordWrap
            lineHeightMode: Label.FixedHeight
            lineHeight: 30
            horizontalAlignment: Label.AlignHCenter
            textFormat: Text.RichText

            onLinkActivated: function (link) {
                Qt.openUrlExternally(link);
            }
        }
        footer_data: CustomDialogButton {
            id: footer_data
            button_text: qsTr("OK")
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            onClicked: {
                dialog.close();
            }
        }
    }

    onClosing: close => {
        if (Recorder.recording) {
            close.accepted = false;
            dialog.title_text = qsTr("Warning");
            content_data.text = qsTr("Recording is in progress; you have to stop recording before closing the application.");
            footer_data.button_text = qsTr("OK");
            dialog.open();
        }
    }

    Connections {
        target: CameraModel
        function onCamera_unplugged_during_recording(name) {
            dialog.camera_name = name;
            dialog.title_text = qsTr("Camera Connection Lost");
            content_data.text = qsTr("Camera “%1” has been disconnected.").arg(dialog.camera_name);
            footer_data.button_text = qsTr("OK");
            dialog.open();
        }
    }

    Connections {
        target: Server
        function onStatus_change(connected) {
            if (!connected && Recorder.recording) {
                dialog.title_text = qsTr("Oops...");
                content_data.text = qsTr("The connection to XDAQ has been lost. Recording will be terminated and all settings will be reset to their defaults.");
                footer_data.button_text = qsTr("Continue");
                Recorder.stop();
                dialog.open();
            }
        }
    }

    Connections {
        target: Server
        function onApi_version_mismatch(version) {
            content_data.anchors.topMargin = 165;
            const mail = "support@kontex.io";
            const website = "https://help.kontex.io/portal/en/newticket";

            dialog.title_text = qsTr("API Version Mismatch");
            content_data.text = qsTr("The current ThorVision server version is %1. " + "Please visit <a href=\"https://developer.kontex.io\">developer.kontex.io</a> " + "to download the <b>XDAQ ThorVision Updater</b>. " + "If the issue persists, contact KonteX Support at " + "<a href=\"mailto:%2\">%2</a> or submit a ticket at " + "<a href=\"%3\">%3</a>.").arg(version).arg(mail).arg(website);
            footer_data.button_text = qsTr("OK");
            dialog.open();
        }
    }
}
