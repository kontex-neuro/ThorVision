import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
    id: record
    color: Color.record_button_background
    border.color: Color.record_button_border
    border.width: 1

    property bool dont_ask_again: false

    Connections {
        target: Recorder
        function onFailed_to_start_recording(message) {
            console.error("Failed to start recording for camera:", message);
            content_data.text = message;
            save_failed_dialog.open();
        }
    }

    Image {
        source: Recorder.recording ? "qrc:/qt/qml/App/Theme/resources/stop-record.svg" : "qrc:/qt/qml/App/Theme/resources/start-record.svg"
        fillMode: Image.PreserveAspectFit
        opacity: CameraModel.all_cameras_streaming ? 1 : 0.5

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -5
    }

    Label {
        text: Recorder.recording ? Recorder.recording_time : qsTr("REC")
        font: AppFont.record
        color: Color.text
        opacity: CameraModel.all_cameras_streaming ? 1 : 0.5

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 30
    }

    AlertDialog {
        id: save_failed_dialog

        title_text: qsTr("Invalid Save Path")
        content_data: Label {
            id: content_data
            anchors.top: parent.top
            anchors.topMargin: 197
            anchors.horizontalCenter: parent.horizontalCenter

            text: qsTr("")
            font: AppFont.popup_text
            color: Color.text
            wrapMode: Text.WordWrap
            lineHeightMode: Text.FixedHeight
            lineHeight: 30
            horizontalAlignment: Text.AlignHCenter
        }
        footer_data: CustomDialogButton {
            id: footer_data
            button_text: qsTr("OK")
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            onClicked: {
                save_failed_dialog.close();
            }
        }
    }

    AlertDialog {
        id: dialog

        title_text: qsTr("Record Settings Confirm")
        content_data: ColumnLayout {
            anchors.centerIn: parent
            spacing: 27

            Label {
                text: qsTr("Are you sure you want to start recording with the following camera settings?")
                font: AppFont.popup_text
                color: Color.text
            }

            Rectangle {
                color: Color.popup_header

                Layout.preferredWidth: 704
                Layout.preferredHeight: 172
                radius: 1

                ListView {
                    model: CameraModel
                    anchors.fill: parent
                    topMargin: 17
                    leftMargin: 17
                    bottomMargin: 17
                    boundsBehavior: Flickable.StopAtBounds
                    focus: true

                    ScrollBar.vertical: ScrollBar {
                        id: scroll_bar
                        policy: ScrollBar.AlwaysOn
                    }

                    delegate: ItemDelegate {
                        id: delegate

                        background: Rectangle {
                            color: Color.popup_header
                            anchors.fill: parent
                        }

                        required property int index
                        property var camera: CameraModel.get(index).camera_item
                        property string name: camera ? camera.name : ""
                        property string cap: camera ? camera.cap : ""
                        property string codec: camera ? camera.codec : ""

                        contentItem: Label {
                            text: qsTr("%1: %2, %3").arg(delegate.name).arg(delegate.cap).arg(delegate.codec)
                            color: Color.text
                            font: AppFont.popup_scroll_text
                        }
                    }
                }
            }
        }
        footer_data: RowLayout {
            anchors.fill: parent

            CustomCheckBox {
                id: dont_ask_again
                text: qsTr("Don’t ask me again")
                font: AppFont.popup_text

                onCheckedChanged: record.dont_ask_again = checked
            }

            Item {
                Layout.fillWidth: true
            }

            CustomDialogButton {
                button_text: qsTr("OK")

                onClicked: {
                    Recorder.start();
                    dialog.close();
                }
            }

            Item {
                Layout.preferredHeight: 13
            }

            CustomDialogButton {
                button_text: qsTr("Cancel")

                onClicked: {
                    dialog.close();
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: true
        hoverEnabled: true

        onClicked: {
            if (!CameraModel.all_cameras_streaming)
                return;

            if (!Recorder.recording) {
                if (record.dont_ask_again) {
                    Recorder.start();
                } else {
                    dialog.open();
                }
            } else {
                Recorder.stop();
            }
        }

        onEntered: {
            if (!CameraModel.all_cameras_streaming)
                return;

            parent.color = Color.hovered_background;
        }

        onExited: {
            parent.color = Color.record_button_background;
        }
    }
}
