pragma ComponentBehavior: Bound
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import Qt.labs.platform 1.1

ApplicationWindow {
    id: window
    visible: true
    title: qsTr("Thor Vision 0.1.5")
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    // width: 1080
    // height: 720

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            id: top
            Layout.preferredHeight: 30
            Layout.fillWidth: true
            spacing: 10
            // Layout.fillHeight: true
            // Layout.minimumHeight: 1

            // Layout.maximumHeight: 150

            StackLayout {
                id: layout
                currentIndex: 0

                Rectangle {
                    color: "transparent"

                    Label {
                        text: qsTr("No Camera Found")
                        horizontalAlignment: Label.AlignHCenter
                        verticalAlignment: Label.AlignVCenter
                        anchors.centerIn: parent
                    }
                }

                ListView {
                    id: camera_list

                    model: ["Camera 1", "Camera 2", "Camera 3", "Camera 4", "Camera 5"]
                    delegate: ItemDelegate {
                        text: modelData
                        width: camera_list.width

                        onClicked: console.log("clicked:", modelData)
                        required property string modelData
                    }
                }
            }

            Rectangle {
                id: num_camera
                Layout.fillWidth: true

                Label {
                    text: qsTr("number of cameras")
                }
                Image {}
            }

            Rectangle {
                Layout.fillWidth: true
            }

            ColumnLayout {
                Layout.fillWidth: true

                Switch {
                    id: split
                    text: qsTr("Split Record")
                }
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Length per Vid:")
                        enabled: split.checked
                    }
                    // SpinBox {
                    //     id: video_length
                    //     from: 1
                    //     to: 9999
                    //     value: 1
                    //     enabled: split.checked
                    // }
                    Slider {
                        from: 1
                        value: 10
                        to: 9999
                        enabled: split.checked
                    }
                    ComboBox {
                        id: time_unit
                        model: ["Sec", "Min", "Hour", "Days"]
                        currentIndex: 0
                        enabled: split.checked
                    }
                }
                RowLayout {
                    Layout.fillWidth: true

                    ComboBox {
                        id: save_path_list
                        model: [save_path_dialog.folder !== "" ? save_path_dialog.folder : "Default Path"]
                    }

                    Button {
                        text: qsTr("...")
                        onClicked: save_path_dialog.open()
                    }

                    FolderDialog {
                        id: save_path_dialog
                        title: qsTr("Select Save Folder")
                        folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                        onAccepted: {
                            save_path_list.model = [folder];
                            save_path_list.currentIndex = 0;
                        }
                    }

                    // Image {
                    //     MouseArea {
                    //         anchors.fill: parent
                    //         onClicked: {
                    //             save_path_dialog.open();
                    //         }
                    //     }
                    // }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true

                Switch {
                    id: loop
                    text: qsTr("Loop")
                }
                RowLayout {
                    Label {
                        text: qsTr("Max Files:")
                        enabled: loop.checked
                    }
                    ComboBox {
                        model: [1, 2, 3, 4, 5]
                        enabled: loop.checked
                    }
                }
                ComboBox {
                    id: dir
                    model: ["[Custom]", "[Auto]"]
                    currentIndex: 0
                }
            }

            ColumnLayout {
                Layout.fillWidth: true

                Button {
                    Image {}
                    // onClicked:
                }
                Label {
                    text: qsTr("REC")
                }
            }

            ColumnLayout {
                Layout.fillWidth: true

                Image {}
                Label {
                    text: qsTr("Connecting...")
                }
                Image {}
            }
        }

        RowLayout {
            id: main
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                // Layout.fillWidth: false
                Layout.preferredWidth: 45
                Layout.fillWidth: true
                // Layout.fillHeight: true

                Button {
                    text: qsTr("1")
                    onClicked: video_stack_layout.currentIndex = 0
                }
                Button {
                    text: qsTr("4")
                    onClicked: video_stack_layout.currentIndex = 1
                }
                Button {
                    text: qsTr("6")
                    onClicked: video_stack_layout.currentIndex = 2
                }
                Button {
                    text: qsTr("12")
                    onClicked: video_stack_layout.currentIndex = 3
                }
            }

            StackLayout {
                id: video_stack_layout
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                // ScrollView {
                //     Layout.fillWidth: true
                //     Layout.fillHeight: true

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Item {
                        // width: Math.min(gridLayout.implicitWidth, parent.width)
                        width: parent.width
                        height: gridLayout.implicitHeight
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        GridLayout {
                            columns: 1
                            Rectangle {
                                Layout.preferredWidth: 160
                                Layout.preferredHeight: 90
                                // Layout.fillWidth: true
                                // Layout.fillHeight: true
                                // width: 160
                                // height: 90
                                color: "white"
                                VideoPreview {
                                    anchors.fill: parent
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Item {
                        width: Math.min(gridLayout.implicitWidth, parent.width)
                        height: gridLayout.implicitHeight
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        GridLayout {
                            columns: 2
                            rows: 2
                            Repeater {
                                model: 4
                                delegate: Rectangle {
                                    // Layout.preferredWidth: parent.width / 2
                                    // Layout.preferredHeight: parent.height / 2
                                    Layout.preferredWidth: 160
                                    Layout.preferredHeight: 90
                                    // width: parent.width / 2
                                    // height: parent.height / 2
                                    // Layout.fillWidth: true
                                    // Layout.fillHeight: true
                                    color: "white"
                                    VideoPreview {
                                        anchors.fill: parent
                                    }
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Item {
                        width: Math.min(gridLayout.implicitWidth, parent.width)
                        height: gridLayout.implicitHeight
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        GridLayout {
                            columns: 3
                            rows: 2
                            Repeater {
                                model: 6
                                delegate: Rectangle {
                                    // Layout.preferredWidth: parent.width / 2
                                    // Layout.preferredHeight: parent.height / 2
                                    Layout.preferredWidth: 160
                                    Layout.preferredHeight: 90

                                    // width: parent.width / 2
                                    // height: parent.height / 2
                                    // Layout.fillWidth: true
                                    // Layout.fillHeight: true
                                    color: "white"
                                    VideoPreview {
                                        anchors.fill: parent
                                    }
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Item {
                        width: Math.min(gridLayout.implicitWidth, parent.width)
                        height: gridLayout.implicitHeight
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        GridLayout {
                            id: gridLayout
                            columns: 4
                            rows: 3

                            Repeater {
                                model: 12
                                delegate: Rectangle {
                                    // width: parent.width / 2
                                    // height: parent.height / 2
                                    Layout.preferredWidth: 160
                                    Layout.preferredHeight: 90

                                    // Layout.preferredWidth: parent.width / 2
                                    // Layout.preferredHeight: parent.height / 2
                                    color: "white"
                                    VideoPreview {
                                        anchors.fill: parent
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Drawer {
            //     id: drawer
            //     width: parent.width * 0.4
            //     height: parent.height
            //     edge: Qt.LeftEdge
            //     modal: true
            //     // width: 0.66 * window.width
            //     // height: window.height

            //     Label {
            //         text: "Hi!"
            //         anchors.centerIn: parent
            //     }
            // }
        }
    }
}
