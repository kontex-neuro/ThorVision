import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Qt.labs.platform

import App.Theme 0.1 as Theme

Item {
    GridLayout {
        anchors.fill: parent
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Item {
            id: record_split

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.row: 0
            Layout.column: 0

            ColumnLayout {
                spacing: 0
                anchors.top: parent.top
                anchors.topMargin: 17
                anchors.left: parent.left
                anchors.leftMargin: 38

                CheckBox {
                    id: split
                    text: qsTr("Split Record")
                    font: Theme.Font.record_settings_text
                    leftPadding: 3

                    Layout.leftMargin: -3
                    // TODO: set text color
                }

                RowLayout {
                    Label {
                        text: qsTr("Length per Vid:")
                        enabled: split.checked
                        font: Theme.Font.record_settings_text
                        color: enabled ? Theme.Color.text : Qt.darker(Theme.Color.text, 2)
                    }

                    SpinBox {
                        from: 1
                        to: 9999
                        value: 1
                        enabled: split.checked
                        editable: true
                        font: Theme.Font.record_settings_dropdown
                        // TODO: set text color

                        Layout.preferredWidth: 78
                    }

                    ComboBox {
                        id: time_unit
                        model: ["Sec", "Min", "Hour", "Day"]
                        currentIndex: 0
                        enabled: split.checked
                        font: Theme.Font.record_settings_dropdown
                        // TODO: set text color

                        Layout.preferredWidth: 75
                        Layout.topMargin: 4
                    }
                }
            }
        }

        Item {
            id: record_loop

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.row: 0
            Layout.column: 1

            ColumnLayout {
                spacing: 5
                anchors.top: parent.top
                anchors.topMargin: 18
                anchors.left: parent.left
                anchors.leftMargin: 25

                CheckBox {
                    id: loop
                    text: qsTr("Loop")
                    font: Theme.Font.record_settings_text
                    leftPadding: 3

                    Layout.leftMargin: -3
                    // TODO: set text color
                }

                RowLayout {
                    Label {
                        text: qsTr("Max Files:")
                        enabled: loop.checked
                        font: Theme.Font.record_settings_text
                        color: enabled ? Theme.Color.text : Qt.darker(Theme.Color.text, 2)
                    }

                    SpinBox {
                        from: 1
                        to: 9999
                        value: 1
                        enabled: loop.checked
                        editable: true
                        font: Theme.Font.record_settings_dropdown
                        // TODO: set text color

                        Layout.preferredWidth: 78
                    }
                }
            }
        }

        Item {
            id: record_path

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.row: 1
            Layout.column: 0
            Layout.columnSpan: 2

            RowLayout {
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 5
                anchors.left: parent.left
                anchors.leftMargin: 30

                ComboBox {
                    id: save_path_list
                    model: [save_path_dialog.folder !== "" ? save_path_dialog.folder : "Default Path"]
                    font: Theme.Font.record_settings_dropdown
                    editable: true
                    // TODO: set text color

                    Layout.preferredWidth: 266
                }

                Button {
                    Layout.alignment: Qt.AlignCenter
                    Layout.preferredWidth: 42
                    // TODO: button height on Windows looks ugly

                    Image {
                        anchors.fill: parent
                        source: "qrc:/select-folder.svg"
                        fillMode: Image.PreserveAspectFit
                    }

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
                    Layout.alignment: Qt.AlignCenter
                    Layout.preferredWidth: 42
                    // TODO: button height on Windows looks ugly

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
                    font: Theme.Font.record_settings_dropdown
                    editable: true
                    // TODO: set text color

                    Layout.preferredWidth: 242

                    // onCurrentIndexChanged: {
                    //     if (currentIndex !== 0) {
                    //         dir.editText = model[currentIndex];
                    //     }
                    // }

                    // contentItem.onFocusChanged: {
                    //     if (!dir.editable || dir.currentIndex !== 0) {
                    //         dir.editText = dir.model[dir.currentIndex];
                    //     }
                    // }

                    // onEditTextChanged: {
                    //     if (editText.length > 20) {
                    //         editText = editText.substring(0, 20);
                    //     }
                    // }

                    // validator: RegularExpressionValidator {
                    //     regularExpression: dir.currentIndex === 0 ? /.*/ : /^.{0,0}$/
                    // }
                }
            }
        }
    }
}
