import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic

import App.Theme 0.1 as Theme

Item {
    id: video_layout

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 66
            Layout.fillHeight: true
            color: "transparent"
            border.color: "white"
            border.width: 1

            ColumnLayout {
                id: video_preview_buttons

                property int selected_index: -1

                // anchors.centerIn: parent
                // Layout.alignment: Qt.AlignTop
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 40
                spacing: 10

                Button {
                    icon.source: "qrc:/preview-1.svg"
                    icon.color: hovered || video_preview_buttons.selected_index == 0 ? Theme.Color.accent : "transparent"
                    icon.width: 21
                    icon.height: 21
                    Layout.alignment: Qt.AlignCenter

                    background: Rectangle {
                        color: "transparent"
                    }

                    onClicked: {
                        video_stack_layout.currentIndex = 0;
                        video_preview_buttons.selected_index = 0;
                        console.log("Clicked 1");
                    }
                }

                Button {
                    icon.source: "qrc:/preview-4.svg"
                    icon.color: hovered || video_preview_buttons.selected_index == 1 ? Theme.Color.accent : "transparent"
                    icon.width: 27
                    icon.height: 27
                    Layout.alignment: Qt.AlignCenter

                    background: Rectangle {
                        color: "transparent"
                    }

                    onClicked: {
                        video_stack_layout.currentIndex = 1;
                        video_preview_buttons.selected_index = 1;
                        console.log("Clicked 4");
                    }
                }

                Button {
                    icon.source: "qrc:/preview-6.svg"
                    icon.color: hovered || video_preview_buttons.selected_index == 2 ? Theme.Color.accent : "transparent"
                    icon.width: 30
                    icon.height: 21
                    Layout.alignment: Qt.AlignCenter

                    background: Rectangle {
                        color: "transparent"
                    }

                    onClicked: {
                        video_stack_layout.currentIndex = 2;
                        video_preview_buttons.selected_index = 2;
                        console.log("Clicked 6");
                    }
                }
                Button {
                    icon.source: "qrc:/preview-12.svg"
                    icon.color: hovered || video_preview_buttons.selected_index == 3 ? Theme.Color.accent : "transparent"
                    icon.width: 30
                    icon.height: 21
                    Layout.alignment: Qt.AlignCenter

                    background: Rectangle {
                        color: "transparent"
                    }

                    onClicked: {
                        video_stack_layout.currentIndex = 3;
                        video_preview_buttons.selected_index = 3;
                        console.log("Clicked 12");
                    }
                }
            }
        }

        Rectangle {
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                id: video_stack_layout
                anchors.fill: parent
                currentIndex: 0

                ScrollView {
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                    Item {
                        width: parent.width
                        implicitHeight: gridLayout_1.implicitHeight

                        GridLayout {
                            id: gridLayout_1
                            columns: 1
                            rowSpacing: 15
                            columnSpacing: 15

                            // anchors.centerIn: parent
                            // anchors.topMargin: 50
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            // anchors.bottomMargin: 50

                            Repeater {
                                model: CameraModel

                                delegate: VideoPreview {
                                    Layout.preferredWidth: 1024
                                    Layout.preferredHeight: 640

                                    required property string name
                                    camera_name: name
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                    Item {
                        width: parent.width
                        implicitHeight: gridLayout_4.implicitHeight

                        GridLayout {
                            id: gridLayout_4
                            columns: 2
                            rows: 2
                            rowSpacing: 15
                            columnSpacing: 15

                            // anchors.centerIn: parent
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter

                            Repeater {
                                model: CameraModel

                                delegate: VideoPreview {
                                    Layout.preferredWidth: 640
                                    Layout.preferredHeight: 320
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                    Item {
                        width: parent.width
                        implicitHeight: gridLayout_6.implicitHeight

                        GridLayout {
                            id: gridLayout_6
                            columns: 3
                            rows: 2
                            rowSpacing: 15
                            columnSpacing: 15

                            // anchors.centerIn: parent
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter

                            Repeater {
                                model: CameraModel

                                delegate: VideoPreview {
                                    Layout.preferredWidth: 400
                                    Layout.preferredHeight: 300
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                    Item {
                        width: parent.width
                        implicitHeight: gridLayout_12.implicitHeight

                        GridLayout {
                            id: gridLayout_12
                            columns: 4
                            rows: 3
                            rowSpacing: 15
                            columnSpacing: 15

                            // anchors.centerIn: parent
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter

                            Repeater {
                                model: CameraModel

                                delegate: VideoPreview {
                                    Layout.preferredWidth: 320
                                    Layout.preferredHeight: 240
                                }
                            }
                        }
                    }
                }
            }
        }

        SettingsView {
            id: settings_view

            Layout.preferredWidth: 250
            // Layout.fillWidth: true
            // Layout.fillHeight: true

            StackLayout {
                currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

                Rectangle {
                    color: "transparent"
                    border.color: "white"
                    border.width: 1

                    ColumnLayout {
                        spacing: 0

                        RowLayout {
                            spacing: 0

                            Image {
                                source: "qrc:/camera-icon.svg"
                                fillMode: Image.PreserveAspectFit
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                            }

                            Label {
                                text: qsTr("No Camera Found")
                                font: Theme.Font.camera_setting_title
                            }
                        }

                        ColumnLayout {
                            spacing: 0
                            enabled: false

                            RowLayout {
                                spacing: 0

                                Label {
                                    text: qsTr("Quality")
                                    font: Theme.Font.camera_setting_label
                                }

                                ComboBox {
                                    font: Theme.Font.camera_option_field
                                    currentIndex: 0
                                    Layout.preferredWidth: 180
                                }
                            }

                            RowLayout {
                                spacing: 0

                                Label {
                                    text: qsTr("Format")
                                    font: Theme.Font.camera_setting_label
                                }

                                ComboBox {
                                    font: Theme.Font.camera_option_field
                                    currentIndex: 0
                                    Layout.preferredWidth: 180
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    color: "transparent"
                    border.color: "white"
                    border.width: 1

                    ColumnLayout {
                        spacing: 0

                        RowLayout {
                            spacing: 0

                            Image {
                                source: "qrc:/camera-icon.svg"
                                fillMode: Image.PreserveAspectFit
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                            }

                            Label {
                                text: qsTr("Camera 1")
                                font: Theme.Font.camera_setting_title
                            }

                            Image {
                                source: "qrc:/change-name.svg"
                                fillMode: Image.PreserveAspectFit
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                            }
                        }

                        ColumnLayout {
                            spacing: 0

                            RowLayout {
                                spacing: 0

                                Label {
                                    text: qsTr("Quality")
                                    font: Theme.Font.camera_setting_label
                                }

                                ComboBox {
                                    model: CameraModel
                                    textRole: "cap"
                                    font: Theme.Font.camera_option_field
                                    currentIndex: 0
                                    Layout.preferredWidth: 180
                                }
                            }

                            RowLayout {
                                spacing: 0

                                Label {
                                    text: qsTr("Format")
                                    font: Theme.Font.camera_setting_label
                                }

                                ComboBox {
                                    model: CameraModel
                                    textRole: "codec"
                                    font: Theme.Font.camera_option_field
                                    currentIndex: 0
                                    Layout.preferredWidth: 180
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
