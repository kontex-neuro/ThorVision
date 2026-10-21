pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// Confirmation before a filesystem check. The SSD is identified when the dialog opens;
// if it is replaced, disconnected or the device goes offline while open, confirming is
// blocked and the user is sent back to the current status.
AlertDialog {
    id: dialog

    objectName: "filesystemCheckDialog"

    signal review_requested

    property string volume_id: ""
    property string attachment: ""
    readonly property var storage: SsdStatus.storage
    readonly property bool same_attachment: SsdStatus.connected && volume_id.length > 0 && volume_id === (storage.uuid || "") && attachment === (storage.generation || "")

    title_text: qsTr("Pause SSD recording and check filesystem?")
    footer_left_margin: 48
    footer_right_margin: 48
    footer_bottom_margin: 30

    onAboutToShow: {
        volume_id = storage.uuid || "";
        attachment = storage.generation || "";
    }

    onAccepted: {
        if (same_attachment && !DeviceRecording.busy)
            DeviceRecording.storage("check");
    }

    content_data: ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.topMargin: 22
        spacing: 14

        Rectangle {
            color: Colour.background_1
            border.color: Colour.border_2
            border.width: 1
            radius: 3

            Layout.fillWidth: true
            Layout.preferredHeight: 34

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14

                Label {
                    text: qsTr("SSD %1 · %2 · %3").arg(dialog.volume_id || qsTr("unknown")).arg(dialog.storage.total_bytes ? SsdStatus.bytes(dialog.storage.total_bytes) : "–").arg(SsdStatus.storage_state)
                    textFormat: Text.PlainText
                    font: AppFont.status_text
                    color: Colour.text
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("%1 cameras recording").arg(SsdStatus.recording_count)
                    font: AppFont.status_text
                    color: Colour.text_muted
                }
            }
        }

        Rectangle {
            visible: !dialog.same_attachment
            color: Colour.critical_panel
            border.color: Colour.critical_panel_border
            border.width: 1
            radius: 3

            Layout.fillWidth: true
            Layout.preferredHeight: changed_text.implicitHeight + 16

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                StatusIcon {
                    kind: "critical"
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                }

                Label {
                    id: changed_text

                    text: SsdStatus.connected ? qsTr("<b>The SSD changed while this dialog was open.</b> It was disconnected or replaced, so the check can't start. Review the current SSD status first.") : qsTr("<b>The device is offline.</b> The check can't start. Review the current SSD status first.")
                    textFormat: Text.StyledText
                    font: AppFont.status_text
                    color: Colour.text
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        ColumnLayout {
            spacing: 11
            opacity: dialog.same_attachment ? 1 : 0.55

            Layout.fillWidth: true

            Repeater {
                model: [
                    { kind: "caution", text: qsTr("<b>All SSD recording pauses</b> for the length of the check.") },
                    { kind: "critical", text: qsTr("Footage during the pause <b>is not recorded to the SSD</b> and can't be recovered later.") },
                    { kind: "off", text: qsTr("Current recording files are finalized before the check starts.") },
                    { kind: "ok", text: qsTr("If the check passes, recording resumes automatically in new files.") },
                    { kind: "off", text: qsTr("The check is read-only. It doesn't reset, format or repair the SSD.") },
                    { kind: "ok", text: qsTr("Camera preview and PC recording keep running.") }
                ]

                delegate: RowLayout {
                    id: fact

                    required property var modelData

                    spacing: 12
                    Layout.fillWidth: true

                    StatusIcon {
                        kind: fact.modelData.kind
                        Layout.preferredWidth: 13
                        Layout.preferredHeight: 13
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 3
                    }

                    Label {
                        text: fact.modelData.text
                        textFormat: Text.StyledText
                        font: AppFont.status_title
                        color: Colour.text
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Keeps the content top-aligned; without it the layout spreads the spare height.
        Item {
            Layout.fillHeight: true
        }
    }

    footer_buttons: [
        CompactButton {
            text: qsTr("Cancel")
            variant: "ghost"
            implicitHeight: 28

            onClicked: dialog.reject()
        },
        CompactButton {
            visible: !dialog.same_attachment
            text: qsTr("Review SSD status")
            implicitHeight: 28

            onClicked: {
                dialog.reject();
                dialog.review_requested();
            }
        },
        CompactButton {
            objectName: "confirmFilesystemCheckButton"
            text: qsTr("Pause SSD recording and check")
            variant: "caution"
            implicitHeight: 28
            enabled: dialog.same_attachment && !DeviceRecording.busy

            onClicked: dialog.accept()
        }
    ]
}
