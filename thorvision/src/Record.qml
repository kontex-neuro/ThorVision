import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: record

    property bool dont_ask_again: false

    Image {
        source: Theme.AppSettings.recording ? "qrc:/stop-record.svg" : "qrc:/start-record.svg"
        fillMode: Image.PreserveAspectFit
        opacity: Theme.AppSettings.camera_detected ? 1 : 0.5

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -5
    }

    Label {
        text: Theme.AppSettings.recording ? Theme.AppSettings.recording_time : qsTr("REC")
        font: Theme.Font.record
        color: Theme.Color.text
        opacity: Theme.AppSettings.camera_detected ? 1 : 0.5

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 30
    }

    AlertDialog {
        id: dialog

        title_text: qsTr("Record Settings Confirm")

        content_data: ColumnLayout {
            anchors.centerIn: parent
            spacing: 27

            Label {
                text: qsTr("Are you sure you want to start recording with the following camera settings?")
                font: Theme.Font.popup_text
                color: Theme.Color.text
            }

            Rectangle {
                color: Theme.Color.popup_header

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

                        width: parent.width
                        background: Rectangle {
                            color: Theme.Color.popup_header
                            anchors.fill: parent
                        }

                        required property int index
                        property var camera: CameraModel.get(index).camera_item
                        property string name: camera ? camera.name : ""
                        property string cap: camera ? camera.cap : ""
                        property string codec: camera ? camera.codec : ""

                        contentItem: Label {
                            text: qsTr("%1: %2, %3").arg(delegate.name).arg(delegate.cap).arg(delegate.codec)
                            color: Theme.Color.text
                            font: Theme.Font.popup_scroll_text
                        }
                    }
                }
            }

            Label {
                text: qsTr("Caution:\nIf you toggle “Loop”, previously recorded files may be overwritten.")
                color: Theme.Color.text
                font: Theme.Font.popup_text
                // TODO: Custom Text
                lineHeightMode: Text.FixedHeight
                lineHeight: 30
            }
        }

        footer_data: RowLayout {
            anchors.fill: parent

            CheckBox {
                id: dont_ask_again
                text: qsTr("Don’t ask me again")
                font: Theme.Font.popup_text

                onCheckedChanged: record.dont_ask_again = checked
            }

            Item {
                Layout.fillWidth: true
            }

            CustomButton {
                button_text: qsTr("OK")

                onClicked: {
                    Recorder.start();
                    dialog.close();
                }
            }

            Item {
                Layout.preferredHeight: 13
            }

            CustomButton {
                button_text: qsTr("Cancel")

                onClicked: {
                    dialog.close();
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: Theme.AppSettings.camera_detected

        onClicked: {
            if (!Theme.AppSettings.recording) {
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
            parent.opacity = 0.5;
        }

        onExited: {
            parent.opacity = 1;
        }
    }
}
