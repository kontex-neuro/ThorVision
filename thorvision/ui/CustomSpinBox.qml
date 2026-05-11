import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 

SpinBox {
    id: box

    font: AppFont.record_settings_dropdown
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
        color: box.editing ? Colour.edit_text : Colour.text
        selectionColor: Colour.accent
        selectedTextColor: Colour.text
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        clip: true

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
        color: box.enabled ? (box.up.pressed ? Colour.down_background : (box.hovered ? Colour.hovered_background : Colour.spinbox_background)) : Colour.spinbox_background
        border.color: box.enabled ? (box.hovered ? Colour.hovered_border : Colour.spinbox_border) : Colour.spinbox_border
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
        color: box.enabled ? (box.down.pressed ? Colour.down_background : (box.hovered ? Colour.hovered_background : Colour.spinbox_background)) : Colour.spinbox_background
        border.color: box.enabled ? (box.hovered ? Colour.hovered_border : Colour.spinbox_border) : Colour.spinbox_border
        border.width: 1

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/spinbox-arrow-down.svg"
            fillMode: Image.PreserveAspectFit
            anchors.centerIn: parent
        }
    }

    background: Rectangle {
        color: box.enabled ? (box.editing ? Colour.edit_background : (box.hovered ? Colour.hovered_background : Colour.spinbox_background)) : Colour.spinbox_background
        border.color: box.enabled ? (box.editing ? Colour.accent : (box.hovered ? Colour.hovered_border : Colour.spinbox_border)) : Colour.spinbox_border
        border.width: 1
    }

    Keys.onEscapePressed: {
        focus = false;
    }
}
