import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Qt.labs.platform

import App.Theme 0.1 as Theme

Item {
    id: record_settings

    GridLayout {
        anchors.fill: parent
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Rectangle {
            id: record_split
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.fillWidth: true
            Layout.fillHeight: true

            Layout.row: 0
            Layout.column: 0

            ColumnLayout {
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 10

                CheckBox {
                    id: split
                    text: qsTr("Split Record")
                    font: Theme.Font.record_setting_label
                }

                RowLayout {

                    Label {
                        text: qsTr("Length per Vid:")
                        enabled: split.checked
                        font: Theme.Font.record_setting_label
                    }

                    SpinBox {
                        id: video_length
                        from: 1
                        to: 9999
                        value: 1
                        enabled: split.checked
                        implicitWidth: 75
                    }

                    ComboBox {
                        id: time_unit
                        model: ["Sec", "Min", "Hour", "Day"]
                        currentIndex: 0
                        enabled: split.checked
                        implicitWidth: 75
                    }
                }
            }
        }

        Rectangle {
            id: record_loop
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.fillWidth: true
            Layout.fillHeight: true

            Layout.row: 0
            Layout.column: 1

            ColumnLayout {
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 5

                CheckBox {
                    id: loop
                    text: qsTr("Loop")
                    font: Theme.Font.record_setting_label
                }

                RowLayout {

                    Label {
                        text: qsTr("Max Files:")
                        enabled: loop.checked
                        font: Theme.Font.record_setting_label
                    }

                    SpinBox {
                        from: 1
                        to: 9999
                        value: 1
                        enabled: loop.checked
                        implicitWidth: 75
                    }
                }
            }
        }

        Rectangle {
            id: record_path
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.fillWidth: true
            Layout.fillHeight: true

            Layout.row: 1
            Layout.column: 0
            Layout.columnSpan: 2

            RowLayout {
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 5

                ComboBox {
                    id: save_path_list
                    Layout.preferredWidth: 300
                    model: [save_path_dialog.folder !== "" ? save_path_dialog.folder : "Default Path"]
                    font: Theme.Font.camera_option_field
                }

                Button {
                    text: qsTr("...")
                    Layout.preferredWidth: 50
                    onClicked: save_path_dialog.open()
                }

                FolderDialog {
                    id: save_path_dialog
                    folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)

                    onAccepted: {
                        save_path_list.model = [folder];
                        save_path_list.currentIndex = 0;
                    }
                }

                Button {
                    Layout.preferredWidth: 50

                    Image {
                        anchors.fill: parent
                        source: "qrc:/open-folder.svg"
                        fillMode: Image.PreserveAspectFit
                    }
                    onClicked: {
                        var path = save_path_dialog.folder;

                        if (path && path !== "") {
                            Qt.openUrlExternally("file://" + path);
                            console.log("Open folder");
                        } else {
                            console.warn("No valid folder path selected.");
                        }
                    }
                }

                ComboBox {
                    id: dir
                    model: ["[Custom]", "[Auto]-YYYY-MM-DD_HH-MM-SS"]
                    currentIndex: 0
                    font: Theme.Font.camera_option_field
                    Layout.preferredWidth: 200
                }
            }
        }
    }
}
