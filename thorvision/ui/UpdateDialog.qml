import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// Device-server update dialog. One instance, driven entirely by Update.state.
AlertDialog {
    id: root

    readonly property int s: Update.state

    readonly property bool offering: s === UpdateState.UpdateAvailable
    readonly property bool working: Update.in_progress
    readonly property bool succeeded: s === UpdateState.Succeeded
    readonly property bool failed: s === UpdateState.Failed

    title_text: {
        if (succeeded)
            return qsTr("Update Complete");
        if (failed)
            return qsTr("Update Failed");
        if (working)
            return qsTr("ThorVision Server Update");
        return Update.required ? qsTr("Server Update Required") : qsTr("Server Update Available");
    }

    // checked.svg, not xdaq-connected.png: the latter is a 110x39 status banner and looked
    // mangled squeezed into the 19x17 title-bar icon slot.
    icon_source: succeeded ? "qrc:/qt/qml/App/Theme/resources/checked.svg" : "qrc:/qt/qml/App/Theme/resources/warning.svg"

    // Never dismissable mid-flight: an accidental Esc during a device transfer is not
    // something we can undo.
    closePolicy: working ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    // Checking deliberately shows nothing -- a modal that appears on every launch before the
    // app knows anything is wrong would be worse than the problem it solves.
    visible: offering || working || succeeded || failed

    onClosed: if (offering) Update.ignore()

    content_data: Item {
        anchors.fill: parent

        // ---------------------------------------------------------------- offer
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 69
            anchors.rightMargin: 68
            anchors.topMargin: 30
            anchors.bottomMargin: 10
            spacing: 16
            visible: root.offering

            Text {
                text: Update.required ? qsTr("A server update is required for this device.") : qsTr("A server update is available for this device.")
                font: AppFont.popup_text
                color: Colour.text
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            // Current and target are always shown together. Because the policy is
            // compatibility-based, the target can legitimately be lower than the device
            // version, and "Update to 0.1.5" alone would read as a bug.
            RowLayout {
                spacing: 12

                Text {
                    text: qsTr("Current %1").arg(Update.device_version)
                    font: AppFont.popup_scroll_text
                    color: Colour.text
                    opacity: 0.8
                }
                Text {
                    text: "→"
                    font: AppFont.popup_text
                    color: Colour.text
                    opacity: 0.8
                }
                Text {
                    text: qsTr("Update to %1").arg(Update.target_version)
                    font: AppFont.popup_scroll_text
                    color: Colour.text
                }
            }

            Text {
                text: qsTr("What's changed")
                font: AppFont.popup_scroll_text
                color: Colour.text
                opacity: 0.8
                visible: Update.changelog.length > 0

                Layout.fillWidth: true
            }

            // Fixed height + scroll: a long changelog must never push the buttons off the
            // fixed-size dialog shell.
            Rectangle {
                color: Colour.log_background
                border.color: Colour.popup_border
                border.width: 1
                radius: 2
                visible: Update.changelog.length > 0

                Layout.fillWidth: true
                Layout.fillHeight: true

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    Text {
                        width: parent.width
                        text: Update.changelog
                        textFormat: Text.MarkdownText
                        font: AppFont.popup_scroll_text
                        color: Colour.text
                        wrapMode: Text.WordWrap

                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }
            }

            Text {
                text: qsTr("Camera streams are paused until you choose. Keep the device connected during the update.")
                font: AppFont.popup_scroll_text
                color: Colour.text
                opacity: 0.8
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }
        }

        // ------------------------------------------------------------- progress
        UpdateProgress {
            anchors.fill: parent
            anchors.leftMargin: 69
            anchors.rightMargin: 68
            anchors.topMargin: 30
            anchors.bottomMargin: 10
            visible: root.working
        }

        // ------------------------------------------------------ success / failure
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 69
            anchors.rightMargin: 68
            anchors.topMargin: 40
            anchors.bottomMargin: 10
            spacing: 16
            visible: root.succeeded || root.failed

            Text {
                text: root.succeeded ? qsTr("The device is now running version %1.").arg(Update.device_version) : Update.error_message
                font: AppFont.popup_text
                color: Colour.text
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            Text {
                text: qsTr("If this problem continues, contact KonteX Support at <a href=\"mailto:support@kontex.io\">support@kontex.io</a> or submit a ticket at <a href=\"https://help.kontex.io/portal/en/newticket\">help.kontex.io</a>.")
                font: AppFont.popup_scroll_text
                color: Colour.text
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                visible: root.failed && Update.recovery === UpdateState.Terminal

                Layout.fillWidth: true

                onLinkActivated: link => Qt.openUrlExternally(link)
            }

            // The log stays available after a failure -- it is the only real diagnostic.
            UpdateLogView {
                model: Update.logs
                visible: root.failed && Update.logs.length > 0

                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    footer_buttons: [
        // -- offer
        CustomDialogButton {
            button_text: qsTr("Ignore")
            visible: root.offering
            onClicked: Update.ignore()
        },
        CustomDialogButton {
            button_text: qsTr("Update Now")
            visible: root.offering
            onClicked: Update.start_update()
        },

        // -- progress
        CustomDialogButton {
            id: cancel_button

            // Only offered while downloading. Once bytes are landing on the device,
            // stopping midway risks a half-written image.
            button_text: qsTr("Cancel")
            visible: root.working

            // Kept enabled so it still reports hover (a disabled Button does not), which is
            // what lets the tooltip explain why cancelling is unavailable. The click is
            // gated instead -- and UpdateController::cancel() ignores it too.
            opacity: Update.cancellable ? 1.0 : 0.45
            onClicked: if (Update.cancellable) Update.cancel()

            // Attached properties rather than a child ToolTip: CustomDialogButton's default
            // property is button_text (a string), so any child declared here is assigned to
            // that and fails to load. The attached form also anchors the tooltip to this
            // button instead of the dialog, which is what makes the hover area correct.
            ToolTip.text: qsTr("Cannot cancel while installing.")
            ToolTip.delay: 300
            ToolTip.visible: !Update.cancellable && hovered
        },

        // -- success
        CustomDialogButton {
            button_text: qsTr("Done")
            visible: root.succeeded
            onClicked: Update.dismiss_result()
        },

        // -- failure
        CustomDialogButton {
            // Closing the app is the user's decision, never ours: even after a rollback they
            // may want to keep working, and the badge stays as the standing reminder.
            button_text: qsTr("Close")
            visible: root.failed
            onClicked: Update.dismiss_result()
        },
        CustomDialogButton {
            button_text: qsTr("Retry")
            visible: root.failed && Update.recovery !== UpdateState.Terminal
            onClicked: Update.start_update()
        }
    ]
}
