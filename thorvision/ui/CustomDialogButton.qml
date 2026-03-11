import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 

Button {
    id: button

    default property alias button_text: content.text

    background: Rectangle {
        color: Color.popup_button
        radius: 2
        border.color: Color.popup_border
        border.width: 1
        implicitWidth: 140
        implicitHeight: 26

        opacity: button.pressed ? 0.5 : (button.hovered ? 0.7 : 1.0)
    }

    contentItem: Label {
        id: content

        font: AppFont.popup_text
        color: Color.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
