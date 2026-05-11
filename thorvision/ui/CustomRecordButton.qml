import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 

Button {
    id: box

    implicitWidth: 22
    implicitHeight: 22
    opacity: box.enabled ? 1.0 : 0.5

    background: Rectangle {
        color: box.enabled ? (box.down ? Colour.down_background : (box.hovered ? Colour.hovered_background : Colour.record_button_background)) : Colour.record_button_background
        border.color: box.enabled ? (box.hovered ? Colour.hovered_border : Colour.record_button_border) : Colour.record_button_border
        border.width: 1
        implicitWidth: box.implicitWidth
        implicitHeight: box.implicitHeight
    }
}
