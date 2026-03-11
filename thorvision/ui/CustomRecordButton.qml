import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 

Button {
    id: box

    implicitWidth: 22
    implicitHeight: 22
    opacity: box.enabled ? 1.0 : 0.5

    background: Rectangle {
        color: box.enabled ? (box.down ? Color.down_background : (box.hovered ? Color.hovered_background : Color.record_button_background)) : Color.record_button_background
        border.color: box.enabled ? (box.hovered ? Color.hovered_border : Color.record_button_border) : Color.record_button_border
        border.width: 1
        implicitWidth: box.implicitWidth
        implicitHeight: box.implicitHeight
    }
}
