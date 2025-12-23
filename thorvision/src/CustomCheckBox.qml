import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 as Theme

CheckBox {
    id: box

    font: Theme.Font.record_settings_text
    checked: false

    opacity: box.enabled ? 1.0 : 0.3

    contentItem: Text {
        text: box.text
        font: box.font
        color: Theme.Color.text
        // opacity: box.enabled ? 1.0 : 0.5
        verticalAlignment: Text.AlignVCenter
        leftPadding: box.indicator.width + box.spacing
    }

    indicator: Rectangle {
        color: box.hovered ? Theme.Color.hovered_background : Theme.Color.checkbox_background
        border.color: box.hovered ? Theme.Color.hovered_checkbox_border : Theme.Color.checkbox_border
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

            color: Theme.Color.checkbox_checked
            visible: box.checked
        }
    }
}
