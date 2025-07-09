import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic

import App.Theme 0.1 as Theme

Item {
    id: setting_view

    // property bool show: true
    default property alias content: setting_area.children

    // anchors.fill: parent

    // Rectangle {
    //     color: "transparent"
    //     border.color: "white"
    //     border.width: 1
    //     anchors.fill: parent

    // RowLayout {
    //     // anchors.fill: parent

    //     width: settings_drawer.width
    //     height: settings_drawer.height

    Rectangle {
        id: view_settings
        color: "black"
        border.width: 1
        border.color: "white"
        visible: true
        // Layout.topMargin: 30
        // Layout.alignment: Qt.AlignTop

        // anchors.left: parent.left
        // anchors.top: parent.top
        // anchors.left: video_stack_layout.right
        // anchors.right: settings_view.left

        Layout.preferredWidth: 20
        Layout.preferredHeight: 20

        // y: 300
        // x: Theme.AppSettings.drawer_visible ? settings_view.x - width : Window.window.width - width

        // Layout.rightMargin: (settings_drawer.visible ? settings_drawer.x - settings_drawer.width : 0)

        Image {
            source: "qrc:/drawer.svg"
            fillMode: Image.PreserveAspectFit
            anchors.fill: parent
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            onClicked: {
                Theme.AppSettings.drawer_visible = !Theme.AppSettings.drawer_visible;
            }
            onEntered: {
                parent.opacity = 0.5;
            }
            onExited: {
                parent.opacity = 1;
            }
        }
    }

    // Button {
    //     // x: (setting_drawer.visible) ? setting_drawer.x - width : Window.window.width - width
    //     // anchors.top: parent.top

    //     // Layout.alignment:
    //     // icon.width: 20
    //     // icon.height: 20
    //     icon.source: "qrc:/drawer.svg"
    //     icon.color: "transparent"

    //     background: Rectangle {
    //         color: "black"
    //         // border.color: "white"
    //         // border.width: 1
    //         // anchors.fill: parent
    //     }

    //     onClicked: {
    //         setting_view.show = !setting_view.show;
    //     }
    // }

    Drawer {
        id: settings_drawer
        modal: false
        edge: Qt.RightEdge
        interactive: false
        visible: Theme.AppSettings.drawer_visible
        // Layout.fillWidth: true
        // Layout.fillHeight: true

        y: 145
        height: Screen.desktopAvailableHeight - 145
        width: 230

        leftInset: -10
        topInset: -20
        // bottomInset: -20
        topMargin: 10

        background: Rectangle {
            color: "transparent"
            border.color: "white"
            border.width: 1
            height: parent.height
        }

        ColumnLayout {
            id: setting_area
            // anchors.top: parent.top
            // anchors.fill: parent
            // Layout.fillWidth: true
            // Layout.fillHeight: true
            // anchors.margins: 10
        }

        // enter: Transition {
        //     NumberAnimation {
        //         property: "position"
        //         to: 1.0
        //         duration: 800
        //         easing.type: Easing.InOutQuad
        //     }
        // }

        // exit: Transition {
        //     NumberAnimation {
        //         property: "position"
        //         to: 0.0
        //         duration: 800
        //         easing.type: Easing.InOutQuad
        //     }
        // }
    }
    // }
}
