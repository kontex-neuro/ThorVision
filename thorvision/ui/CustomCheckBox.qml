import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 

CheckBox {
    id: box

    font: AppFont.record_settings_text
    checked: false

    opacity: box.enabled ? 1.0 : 0.3

    contentItem: Text {
        text: box.text
        font: box.font
        color: Colour.text
        // opacity: box.enabled ? 1.0 : 0.5
        verticalAlignment: Text.AlignVCenter
        leftPadding: box.indicator.width + box.spacing
    }

    indicator: Rectangle {
        color: box.hovered ? Colour.hovered_background : Colour.checkbox_background
        border.color: box.hovered ? Colour.hovered_checkbox_border : Colour.checkbox_border
        border.width: 1
        radius: 2

        implicitWidth: 15
        implicitHeight: implicitWidth
        x: box.leftPadding
        y: parent.height / 2 - height / 2

        Rectangle {
            width: 7
            height: width
            x: 4
            y: x

            color: Colour.checkbox_checked
            visible: box.checked
        }
    }
}
