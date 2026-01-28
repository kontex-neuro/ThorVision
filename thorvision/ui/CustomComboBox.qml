pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Fusion

import App.Theme 0.1 as Theme

ComboBox {
    id: box

    font: Theme.Font.camera_settings_dropdown
    implicitWidth: 172
    implicitHeight: 22
    opacity: box.enabled ? 1.0 : 0.5

    delegate: ItemDelegate {
        id: delegate

        required property var model

        leftPadding: 5
        topPadding: 4
        bottomPadding: 3
        width: box.width

        contentItem: Text {
            text: delegate.model[box.textRole]
            color: Theme.Color.text
            font: box.font

            elide: Text.ElideRight
            maximumLineCount: 1

            verticalAlignment: Text.AlignVCenter
        }
        highlighted: ListView.isCurrentItem
        background: Rectangle {
            color: delegate.highlighted ? Theme.Color.accent : "transparent"
        }
    }

    contentItem: Item {
        width: box.width
        height: box.height

        Text {
            visible: !box.editable
            text: box.editable ? box.editText : box.displayText
            font: box.font
            color: Theme.Color.text
            elide: Text.ElideRight
            maximumLineCount: 1

            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
        }
    }

    background: Rectangle {
        color: box.enabled ? (box.down ? Theme.Color.down_background : (box.hovered ? Theme.Color.hovered_background : Theme.Color.dropdown_background)) : Theme.Color.dropdown_background
        border.color: box.enabled ? (box.hovered ? Theme.Color.hovered_border : Theme.Color.dropdown_border) : Theme.Color.dropdown_border
        border.width: 1
    }

    indicator: Rectangle {
        border.color: box.enabled ? (box.hovered ? Theme.Color.hovered_border : Theme.Color.dropdown_border) : Theme.Color.dropdown_border
        color: box.enabled ? (box.hovered ? Theme.Color.hovered_background : Theme.Color.dropdown_background) : Theme.Color.dropdown_background
        width: 18
        height: box.height
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        // visible: !box.editable || !textInput.activeFocus

        Image {
            anchors.centerIn: parent
            source: "qrc:/qt/qml/App/Theme/resources/arrow-down.svg"
            fillMode: Image.PreserveAspectFit
        }
    }

    popup: Popup {
        // y: box.height
        width: box.width

        property real available_space_below: box.Window.height - box.mapToGlobal(0, box.height).y - bottomMargin
        property real available_space_above: box.mapToGlobal(0, 0).y - topMargin

        property bool show_above: available_space_below < 200 && available_space_above > available_space_below

        y: show_above ? -height : box.height
        height: {
            if (show_above) {
                return Math.min(contentItem.implicitHeight, available_space_above);
            } else {
                return Math.min(contentItem.implicitHeight, available_space_below);
            }
        }

        leftPadding: 1
        rightPadding: 1
        topPadding: 0
        bottomPadding: 0

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: box.popup.visible ? box.delegateModel : null
            currentIndex: box.highlightedIndex

            ScrollBar.vertical: ScrollBar {
                id: scroll_bar
                policy: ScrollBar.AlwaysOn
            }
        }

        background: Rectangle {
            color: Theme.Color.dropdown_background
            border.color: Theme.Color.dropdown_border
            radius: 1
        }
    }

    Keys.onEscapePressed: {
        focus = false;
    }
}
