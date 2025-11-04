import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 as Theme

Button {
    id: box

    implicitWidth: 22
    implicitHeight: 22

    background: Rectangle {
        color: box.down ? Theme.Color.down_background : (box.hovered ? Theme.Color.hovered_background : Theme.Color.record_button_background)
        border.color: box.hovered ? Theme.Color.hovered_border : Theme.Color.record_button_border
        border.width: 1
        implicitWidth: box.implicitWidth
        implicitHeight: box.implicitHeight
    }
}
