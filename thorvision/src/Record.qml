import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import App.Theme 0.1 as Theme

Item {
    id: record

    Image {
        source: Theme.AppSettings.recording ? "qrc:/stop-record.svg" : "qrc:/start-record.svg"
        fillMode: Image.PreserveAspectFit
        opacity: Theme.AppSettings.camera_detected ? 1 : 0.5

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -5
    }

    Timer {
        id: timer
        interval: 1000
        repeat: true
        running: Theme.AppSettings.recording
        onTriggered: Theme.AppSettings.recording_time += 1
    }

    Label {
        text: Theme.AppSettings.recording ? Theme.AppSettings.formatted_recording_time : qsTr("REC")
        font: Theme.Font.record
        color: Theme.Color.text
        opacity: Theme.AppSettings.camera_detected ? 1 : 0.5

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 30
    }

    Dialog {
        id: recordConfirmDialog
        modal: true
        title: qsTr("Record Settings Confirm")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay

        // Outputs
        property alias dontAskAgain: dontAskAgainCheckBox.checked
        // property alias accepted: recordConfirmDialog.accepted

        property var cameraDescriptions: ["Camera 1 - USB CAMERA: Full HD @ 120FPS, M-JPEG", "Camera 2 - USB CAMERA: Full HD @ 120FPS, M-JPEG", "Camera 3 - USB CAMERA: Full HD @ 120FPS, M-JPEG", "Camera 4 - USB CAMERA: Full HD @ 120FPS, M-JPEG"]

        header: Rectangle {
            color: "black"

            Layout.preferredWidth: 400
            Layout.preferredHeight: 300
        }

        contentItem: Rectangle {
            color: "gray"

            Layout.preferredWidth: 400
            Layout.preferredHeight: 300

            ColumnLayout {
                anchors.fill: parent
                spacing: 20

                Label {
                    text: qsTr("Are you sure you want to start recording with the following camera settings?")
                    font.pixelSize: 16
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ListView {
                    model: recordConfirmDialog.cameraDescriptions
                    Layout.preferredHeight: 100
                    clip: true
                    Layout.fillWidth: true

                    delegate: Label {
                        text: modelData
                        color: "lightgray"
                        padding: 4
                    }

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                    }
                }

                Label {
                    text: qsTr("Caution:\nIf you toggle “Loop”, previously recorded files may be overwritten.")
                    color: "orange"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        footer: Rectangle {
            color: "black"

            Layout.preferredWidth: 400
            Layout.preferredHeight: 300

            CheckBox {
                id: dontAskAgainCheckBox
                text: qsTr("Don’t ask me again")
            }
        }

        onAccepted: console.log("Ok clicked")
        onRejected: console.log("Cancel clicked")
    }

    MessageDialog {
        id: confirm
        title: qsTr("Record Settings Confirm")
        text: qsTr("Are you sure you want to start recording?")
        visible: false
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        modality: Qt.ApplicationModal

        onAccepted: {
            Theme.AppSettings.recording_time = 0;
            Theme.AppSettings.recording = true;
            timer.start();
            console.log("Recording started");
        }

        onRejected: {
            console.log("Recording cancelled");
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: Theme.AppSettings.camera_detected

        onClicked: {
            // recordConfirmDialog.open();
            // confirm.open();
            if (Theme.AppSettings.recording) {
                timer.stop();
            } else {
                timer.start();
            }
            Theme.AppSettings.recording = !Theme.AppSettings.recording;
            Theme.AppSettings.recording_time = 0;
            console.log("recording: ", Theme.AppSettings.recording);
        }

        onEntered: {
            parent.opacity = 0.5;
        }

        onExited: {
            parent.opacity = 1;
        }
    }
}
