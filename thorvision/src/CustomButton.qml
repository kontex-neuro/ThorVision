import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 as Theme

Button {
    id: button

    default property alias button_text: content.text

    background: Rectangle {
        color: Theme.Color.popup_button
        radius: 2
        border.color: Theme.Color.popup_border
        border.width: 1
        implicitWidth: 140
        implicitHeight: 26

        opacity: button.pressed ? 0.5 : (button.hovered ? 0.7 : 1.0)

        // Behavior on opacity {
        //     NumberAnimation {
        //         duration: 100
        //     }
        // }
    }

    contentItem: Label {
        id: content

        font: Theme.Font.popup_normal_text
        color: Theme.Color.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
