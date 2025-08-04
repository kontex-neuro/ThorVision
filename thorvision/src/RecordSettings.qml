pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import App.Theme 0.1 as Theme

Item {
    id: settings

    property var recorder_settings: RecorderSettings

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
                    checked: settings.recorder_settings.split_enabled

                    Layout.leftMargin: -3
                    // TODO: set text color

                    onClicked: {
                        settings.recorder_settings.split_enabled = checked;
                    }
                }

                RowLayout {
                    Label {
                        text: qsTr("Length per Vid:")
                        enabled: split.checked
                        font: Theme.Font.record_settings_text
                        color: enabled ? Theme.Color.text : Qt.darker(Theme.Color.text, 2)
                    }

                    SpinBox {
                        id: split_length
                        from: 1
                        to: 9999
                        enabled: split.checked
                        editable: true
                        font: Theme.Font.record_settings_dropdown
                        // TODO: set text color

                        Layout.preferredWidth: 78

                        onValueChanged: {
                            settings.recorder_settings.split_length = value;
                        }
                    }

                    ComboBox {
                        id: time_unit
                        model: ["Sec", "Min", "Hour", "Day"]
                        enabled: split.checked
                        font: Theme.Font.record_settings_dropdown
                        hoverEnabled: true
                        // TODO: set text color

                        Layout.preferredWidth: 75
                        Layout.topMargin: 4

                        onCurrentIndexChanged: {
                            settings.recorder_settings.split_unit_index = currentIndex;
                        }
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
                    checked: settings.recorder_settings.loop_enabled
                    enabled: split.checked

                    Layout.leftMargin: -3
                    // TODO: set text color

                    onCheckedChanged: {
                        settings.recorder_settings.loop_enabled = checked;
                    }
                }

                RowLayout {
                    Label {
                        text: qsTr("Max Files:")
                        enabled: loop.checked
                        font: Theme.Font.record_settings_text
                        color: enabled ? Theme.Color.text : Qt.darker(Theme.Color.text, 2)
                    }

                    SpinBox {
                        id: max_files
                        from: 1
                        to: 9999
                        enabled: loop.checked
                        editable: true
                        font: Theme.Font.record_settings_dropdown
                        // TODO: set text color

                        Layout.preferredWidth: 78

                        onValueChanged: {
                            settings.recorder_settings.max_files = value;
                        }
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
                    model: settings.recorder_settings.save_paths
                    font: Theme.Font.record_settings_dropdown
                    hoverEnabled: true
                    // TODO: set text color

                    Layout.preferredWidth: 266

                    onCurrentIndexChanged: {
                        settings.recorder_settings.update_save_path_history(save_path_dialog.selectedFolder);
                    }
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
                    currentFolder: settings.recorder_settings.save_paths[0]

                    onAccepted: {
                        var path = save_path_dialog.selectedFolder.toString().replace(/^(file:\/{2})/, "");

                        settings.recorder_settings.update_save_path_history(path);
                        save_path_list.currentIndex = settings.recorder_settings.save_paths.indexOf(path);
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
                        var path = save_path_dialog.currentFolder;

                        if (!path) {
                            console.warn("No valid folder path selected.");
                            return;
                        }
                        console.log("Opening folder:", path);
                        Qt.openUrlExternally(path);
                    }
                }

                ComboBox {
                    id: dir

                    model: ListModel {
                        id: dir_model

                        ListElement {
                            type: "Custom"
                            label: "directory_name"
                        }
                        ListElement {
                            type: "Auto"
                            label: "YYYY-MM-DD_HH-MM-SS"
                        }
                    }
                    textRole: "label"
                    font: Theme.Font.record_settings_dropdown
                    displayText: currentText
                    editable: !settings.recorder_settings.dir_date
                    hoverEnabled: true
                    // TODO: set text color

                    Layout.preferredWidth: 242

                    delegate: ItemDelegate {
                        id: delegate

                        required property var model

                        width: dir.width

                        highlighted: ListView.isCurrentItem
                        // background: Rectangle {
                        //     color: delegate.highlighted ? Theme.Color.accent : "transparent"
                        // }

                        contentItem: Text {
                            text: (delegate.model.type === "Custom" ? "[Custom] " : "[Auto] ") + delegate.model.label
                            color: Theme.Color.text
                            font: Theme.Font.record_settings_dropdown
                            elide: Text.ElideRight
                        }
                    }

                    onEditTextChanged: {
                        if (!settings.recorder_settings.dir_date) {
                            settings.recorder_settings.dir_name = editText;
                            dir_model.setProperty(currentIndex, "label", settings.recorder_settings.dir_name);
                        }
                    }

                    onCurrentIndexChanged: {
                        if (!settings.recorder_settings.dir_date) {
                            dir.editText = settings.recorder_settings.dir_name;
                        } else {
                            settings.recorder_settings.dir_name = model.get(1).label;
                        }
                        settings.recorder_settings.dir_date = currentIndex === 1;
                    }
                }
            }
        }
    }
}
