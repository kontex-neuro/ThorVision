import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: setting_view

    // property bool show: true
    // default property alias content: setting_area.children

    // Rectangle {
    //     color: "transparent"
    //     border.color: "white"
    //     border.width: 1
    //     anchors.fill: parent

    // RowLayout {
    //     // anchors.fill: parent

    //     Layout.preferredWidth: settings_drawer.width
    //     Layout.preferredHeight: settings_drawer.height

    //     // width: settings_drawer.width
    //     // height: settings_drawer.height

    //     Rectangle {
    //         id: view_settings
    //         color: "black"
    //         border.width: 1
    //         border.color: "white"
    //         visible: true
    //         // Layout.topMargin: 30
    //         // Layout.alignment: Qt.AlignTop

    //         // anchors.left: parent.left
    //         // anchors.top: parent.top
    //         // anchors.left: video_stack_layout.right
    //         // anchors.right: settings_view.left

    //         Layout.preferredWidth: 20
    //         Layout.preferredHeight: 20

    //         // y: 300
    //         // x: Theme.AppSettings.drawer_visible ? settings_view.x - width : Window.window.width - width
    //         // Layout.rightMargin: (settings_drawer.visible ? settings_drawer.x - settings_drawer.width : 0)

    //         Image {
    //             source: "qrc:/drawer.svg"
    //             fillMode: Image.PreserveAspectFit
    //             anchors.fill: parent
    //         }

    //         MouseArea {
    //             anchors.fill: parent
    //             hoverEnabled: true

    //             onClicked: {
    //                 Theme.AppSettings.drawer_visible = !Theme.AppSettings.drawer_visible;
    //             }
    //             onEntered: {
    //                 parent.opacity = 0.5;
    //             }
    //             onExited: {
    //                 parent.opacity = 1;
    //             }
    //         }
    //     }

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
        visible: Theme.AppSettings.camera_settings_visible

        y: 111 + 19 + 2
        height: Screen.desktopAvailableHeight - y
        width: 265

        background: Rectangle {
            color: "transparent"
            anchors.fill: parent
        }

        Item {
            ColumnLayout {
                anchors.top: parent.top
                anchors.topMargin: 26
                anchors.left: parent.left
                anchors.leftMargin: 16
                spacing: 36

                RowLayout {
                    spacing: 0

                    Image {
                        source: "qrc:/camera-icon.svg"
                        fillMode: Image.PreserveAspectFit

                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 15
                    }

                    StackLayout {
                        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

                        Item {
                            Label {
                                text: qsTr("No Camera Found")
                                font: Theme.Font.camera_setting_title
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                            }
                        }

                        Item {
                            id: camera_name
                            // property string cameraName: CameraModel.name(Theme.AppSettings.selected_camera_index)
                            property bool editing: false

                            Layout.preferredWidth: 90
                            Layout.fillHeight: true

                            RowLayout {
                                anchors.fill: parent
                                anchors.left: parent.left
                                anchors.leftMargin: 12

                                Label {
                                    visible: !camera_name.editing
                                    text: connected_camera_list.currentItem.name
                                    font: Theme.Font.camera_setting_title
                                }

                                TextInput {
                                    id: editor

                                    visible: camera_name.editing
                                    text: CameraModel.name(Theme.AppSettings.selected_camera_index)
                                    font: Theme.Font.camera_setting_title
                                    focus: camera_name.editing
                                    color: Theme.Color.text_1

                                    onAccepted: {
                                        console.log("onAccepted");
                                        // camera_name.cameraName = editor.text;
                                        camera_name.editing = false;
                                        CameraModel.set_name(Theme.AppSettings.selected_camera_index, editor.text);
                                    }
                                    onFocusChanged: {
                                        console.log("onFocusChanged");
                                        if (!focus) {
                                            camera_name.editing = false;
                                            // camera_name.cameraName = editor.text;
                                        }
                                    }
                                }

                                Image {
                                    source: "qrc:/change-name.svg"
                                    fillMode: Image.PreserveAspectFit
                                    visible: !camera_name.editing

                                    Layout.preferredWidth: 15
                                    Layout.preferredHeight: 15
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton

                                onClicked: {
                                    camera_name.editing = true;
                                    editor.focus = true;
                                    editor.selectAll();
                                }
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        spacing: 0
                        enabled: Theme.AppSettings.camera_detected ? true : false

                        RowLayout {
                            spacing: 10

                            Label {
                                text: qsTr("Quality")
                                font: Theme.Font.camera_setting_label
                                color: Theme.Color.text_1
                            }

                            ComboBox {
                                model: CameraModel.caps(Theme.AppSettings.selected_camera_index)
                                font: Theme.Font.camera_option_field
                                currentIndex: 0

                                Layout.preferredWidth: 172
                            }
                        }

                        RowLayout {
                            spacing: 10

                            Label {
                                text: qsTr("Format")
                                font: Theme.Font.camera_setting_label
                                color: Theme.Color.text_1
                            }

                            ComboBox {
                                model: CameraModel.codecs(Theme.AppSettings.selected_camera_index)
                                font: Theme.Font.camera_option_field
                                // TODO: set text color
                                currentIndex: 0

                                Layout.preferredWidth: 172
                            }
                        }
                    }
                }
            }
        }
    }
}
