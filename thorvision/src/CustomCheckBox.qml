import QtQuick
import QtQuick.Controls.Basic

import App.Theme 0.1 as Theme

CheckBox {
    id: box

    font: Theme.Font.record_settings_text
    checked: false

    opacity: box.enabled ? 1.0 : 0.3

    contentItem: Text {
        text: box.text
        font: box.font
        // color: box.enabled ? Theme.Color.checkbox_background : Theme.Color.disabled_background
        color: Theme.Color.checkbox_background
        // opacity: box.enabled ? 1.0 : 0.5
        verticalAlignment: Text.AlignVCenter
        leftPadding: box.indicator.width + box.spacing
    }

    indicator: Rectangle {
        // color: box.enabled ? Theme.Color.checkbox_background : Theme.Color.disabled_background
        color: Theme.Color.checkbox_background
        // border.color: box.enabled ? Theme.Color.checkbox_border : Theme.Color.disabled_border
        border.color: Theme.Color.checkbox_border
        border.width: 1
        radius: 2
        // opacity: box.enabled ? 1.0 : 0.5

        implicitWidth: 12
        implicitHeight: 12
        x: box.leftPadding
        y: parent.height / 2 - height / 2

        Rectangle {
            width: 7
            height: 7
            x: 2.5
            y: 2.5
            // color: box.enabled ? Theme.Color.checkbox_checked : Theme.Color.disabled_text
            color: Theme.Color.checkbox_checked
            visible: box.checked
            // opacity: box.enabled ? 1.0 : 0.5
        }
    }
}
