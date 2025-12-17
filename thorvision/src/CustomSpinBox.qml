import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 as Theme

SpinBox {
    id: box

    font: Theme.Font.record_settings_dropdown
    editable: true
    implicitWidth: 55
    implicitHeight: 22
    opacity: box.enabled ? 1.0 : 0.5

    from: 1
    to: 9999
    value: 1

    property bool editing: false

    contentItem: TextInput {
        text: box.value
        font: box.font
        color: box.editing ? Theme.Color.edit_text : Theme.Color.text
        selectionColor: Theme.Color.accent
        selectedTextColor: Theme.Color.text
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter

        validator: RegularExpressionValidator {
            regularExpression: /^[1-9]\d{0,3}$/
        }

        onEditingFinished: {
            box.editing = false;
            box.focus = false;
        }
        onActiveFocusChanged: {
            box.editing = activeFocus;
            if (text.length === 0) {
                box.value = 1;
                text = box.value;
            }
        }
    }

    up.indicator: Rectangle {
        x: parent.width - width
        implicitWidth: 11
        implicitHeight: 11
        color: box.enabled ? (box.up.pressed ? Theme.Color.down_background : (box.hovered ? Theme.Color.hovered_background : Theme.Color.spinbox_background)) : Theme.Color.spinbox_background
        border.color: box.enabled ? (box.hovered ? Theme.Color.hovered_border : Theme.Color.spinbox_border) : Theme.Color.spinbox_border
        border.width: 1

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/spinbox-arrow-up.svg"
            fillMode: Image.PreserveAspectFit
            anchors.centerIn: parent
        }
    }

    down.indicator: Rectangle {
        x: parent.width - width
        y: parent.height - height
        implicitWidth: 11
        implicitHeight: 11
        color: box.enabled ? (box.down.pressed ? Theme.Color.down_background : (box.hovered ? Theme.Color.hovered_background : Theme.Color.spinbox_background)) : Theme.Color.spinbox_background
        border.color: box.enabled ? (box.hovered ? Theme.Color.hovered_border : Theme.Color.spinbox_border) : Theme.Color.spinbox_border
        border.width: 1

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/spinbox-arrow-down.svg"
            fillMode: Image.PreserveAspectFit
            anchors.centerIn: parent
        }
    }

    background: Rectangle {
        color: box.enabled ? (box.editing ? Theme.Color.edit_background : (box.hovered ? Theme.Color.hovered_background : Theme.Color.spinbox_background)) : Theme.Color.spinbox_background
        border.color: box.enabled ? (box.editing ? Theme.Color.accent : (box.hovered ? Theme.Color.hovered_border : Theme.Color.spinbox_border)) : Theme.Color.spinbox_border
        border.width: 1
    }

    Keys.onEscapePressed: {
        focus = false;
    }
}
