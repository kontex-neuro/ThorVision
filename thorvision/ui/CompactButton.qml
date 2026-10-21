import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1

// Text button for dense panels and dialog footers. CustomDialogButton's 18px label is
// sized for single-action alerts; this one fits several actions in a row.
// variant: "default" | "primary" | "caution" | "ghost"
Button {
    id: button

    property string variant: "default"

    font: AppFont.status_text
    implicitHeight: 26
    leftPadding: 12
    rightPadding: 12
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: 2
        border.width: button.visualFocus ? 2 : 1
        border.color: {
            if (button.visualFocus)
                return Colour.text;
            switch (button.variant) {
            case "primary":
                return Colour.accent;
            case "caution":
                return Colour.status_caution;
            case "ghost":
                return Colour.border_1;
            }
            return Colour.popup_border;
        }
        color: {
            switch (button.variant) {
            case "primary":
                return Colour.accent_fill;
            case "caution":
                return Colour.caution_panel;
            case "ghost":
                return "transparent";
            }
            return Colour.popup_button;
        }
        opacity: !button.enabled ? 0.45 : (button.pressed ? 0.6 : (button.hovered ? 0.8 : 1.0))
    }

    contentItem: Label {
        text: button.text
        font: button.font
        color: Colour.text
        opacity: button.enabled ? 1.0 : 0.6
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
