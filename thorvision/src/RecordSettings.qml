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

                CustomCheckBox {
                    id: split
                    text: qsTr("Split Record")
                    leftPadding: 3

                    Layout.leftMargin: -3

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

                    CustomSpinBox {
                        from: 1
                        to: 9999
                        enabled: split.checked

                        Layout.preferredWidth: 62

                        onValueChanged: {
                            settings.recorder_settings.split_length = value;
                        }
                    }

                    CustomComboBox {
                        id: time_unit
                        model: [qsTr("Sec"), qsTr("Min"), qsTr("Hour"), qsTr("Day")]
                        enabled: split.checked

                        Layout.preferredWidth: 62

                        onCurrentIndexChanged: {
                            settings.recorder_settings.split_unit_index = currentIndex;
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.row: 0
            Layout.column: 1

            ColumnLayout {
                spacing: 0
                anchors.top: parent.top
                anchors.topMargin: 17
                anchors.left: parent.left
                anchors.leftMargin: 60

                CustomCheckBox {
                    id: loop
                    text: qsTr("Loop")
                    leftPadding: 3
                    enabled: split.checked

                    Layout.leftMargin: -3

                    onCheckedChanged: {
                        settings.recorder_settings.loop_enabled = checked;
                    }
                }

                RowLayout {
                    Label {
                        text: qsTr("Max Files:")
                        enabled: loop.checked && split.checked
                        font: Theme.Font.record_settings_text
                        color: enabled ? Theme.Color.text : Qt.darker(Theme.Color.text, 2)
                    }

                    CustomSpinBox {
                        from: 1
                        to: 9999
                        enabled: loop.checked && split.checked

                        Layout.preferredWidth: 62

                        onValueChanged: {
                            settings.recorder_settings.max_files = value;
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.row: 1
            Layout.column: 0
            Layout.columnSpan: 2

            RowLayout {
                spacing: 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 15
                anchors.left: parent.left
                anchors.leftMargin: 37

                CustomComboBox {
                    id: save_path_list
                    model: settings.recorder_settings.save_paths

                    Layout.preferredWidth: 280

                    onCurrentIndexChanged: {
                        settings.recorder_settings.update_save_path_history(save_path_dialog.selectedFolder);
                    }
                }

                RowLayout {
                    spacing: 17

                    RowLayout {
                        spacing: 3

                        CustomRecordButton {
                            Image {
                                source: "qrc:/select-folder.svg"
                                fillMode: Image.PreserveAspectFit
                                anchors.centerIn: parent
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
                                // console.log("save_path_list.currentIndex", settings.recorder_settings.save_paths[0]);
                            }
                        }

                        CustomRecordButton {
                            Image {
                                source: "qrc:/open-folder.svg"
                                fillMode: Image.PreserveAspectFit
                                anchors.centerIn: parent
                            }

                            onClicked: {
                                var path = settings.recorder_settings.save_paths[0];

                                if (!path) {
                                    console.warn("No valid folder path selected.");
                                    return;
                                }
                                console.log("Opening folder:", path);
                                Qt.openUrlExternally(path);
                            }
                        }
                    }

                    CustomComboBox {
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
                        displayText: currentText
                        editable: !settings.recorder_settings.dir_date

                        Layout.preferredWidth: 233

                        delegate: ItemDelegate {
                            id: delegate

                            required property var model

                            width: dir.width
                            highlighted: ListView.isCurrentItem
                            background: Rectangle {
                                color: delegate.highlighted ? Theme.Color.accent : "transparent"
                            }
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
}
