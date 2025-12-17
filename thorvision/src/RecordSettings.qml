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
            enabled: !Theme.AppSettings.recording

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
                        settings.recorder_settings.split_on = checked;
                        forceActiveFocus(Qt.NoFocusReason);
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
                    enabled: !Theme.AppSettings.recording

                    Layout.preferredWidth: 280

                    onCurrentIndexChanged: {
                        var path = settings.recorder_settings.save_paths[currentIndex];
                        settings.recorder_settings.update_save_path_history(path);
                    }
                }

                RowLayout {
                    spacing: 17

                    RowLayout {
                        spacing: 3

                        CustomRecordButton {
                            enabled: !Theme.AppSettings.recording

                            Image {
                                source: "qrc:/qt/qml/App/Theme/resources/select-folder.svg"
                                fillMode: Image.PreserveAspectFit
                                anchors.centerIn: parent
                            }

                            onClicked: {
                                save_path_dialog.open();
                            }
                        }

                        FolderDialog {
                            id: save_path_dialog
                            currentFolder: settings.recorder_settings.save_paths[0]

                            onAccepted: {
                                if (Qt.platform.os === "windows") {
                                    var path = save_path_dialog.selectedFolder.toString().replace(/^(file:\/{3})/, "");
                                } else {
                                    var path = save_path_dialog.selectedFolder.toString().replace(/^(file:\/{2})/, "");
                                }
                                settings.recorder_settings.update_save_path_history(path);
                            }
                        }

                        CustomRecordButton {
                            Image {
                                source: "qrc:/qt/qml/App/Theme/resources/open-folder.svg"
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
                                if (Qt.platform.os === "windows") {
                                    Qt.openUrlExternally("file:///" + path);
                                } else {
                                    Qt.openUrlExternally("file://" + path);
                                }
                            }
                        }
                    }

                    CustomComboBox {
                        id: dir
                        model: ListModel {
                            id: dir_model

                            ListElement {
                                type: "Auto"
                                label: "YYYY-MM-DD_HH-MM-SS"
                            }
                            ListElement {
                                type: "Custom"
                                label: "Experiment Name"
                            }
                        }
                        textRole: "label"
                        editable: !settings.recorder_settings.dir_date
                        enabled: !Theme.AppSettings.recording

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

                        contentItem: Item {
                            width: dir.width
                            height: dir.height

                            Text {
                                text: dir.editable ? dir.editText : dir.displayText
                                visible: !dir.editable || !text_input.activeFocus
                                font: dir.font
                                color: Theme.Color.text
                                elide: Text.ElideRight
                                maximumLineCount: 1

                                anchors.fill: parent
                                anchors.leftMargin: 5
                                anchors.rightMargin: 5
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                            }

                            TextInput {
                                id: text_input

                                text: dir.editable ? dir.editText : dir.displayText
                                visible: dir.editable
                                font: dir.font
                                color: dir.editing ? Theme.Color.edit_text : Theme.Color.text
                                selectionColor: Theme.Color.accent
                                selectedTextColor: Theme.Color.text

                                anchors.fill: parent
                                anchors.leftMargin: 5
                                anchors.rightMargin: 5
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft

                                validator: RegularExpressionValidator {
                                    regularExpression: /^[a-zA-Z0-9_ ]+$/
                                }

                                onTextChanged: {
                                    if (dir.editable) {
                                        dir.editText = text;
                                    }
                                }
                                onEditingFinished: {
                                    console.log("onEditingFinished", dir.editText);
                                    dir.editing = false;
                                    dir.focus = false;
                                    settings.recorder_settings.dir_name = dir.editText;
                                    dir_model.setProperty(dir.currentIndex, "label", settings.recorder_settings.dir_name);
                                }
                                onActiveFocusChanged: {
                                    console.log("onActiveFocusChanged", activeFocus);
                                    dir.editing = activeFocus;
                                    if (dir.editText.length === 0) {
                                        dir.editText = qsTr("Untitled folder");
                                    }
                                    settings.recorder_settings.dir_name = dir.editText;
                                    dir_model.setProperty(dir.currentIndex, "label", settings.recorder_settings.dir_name);
                                }
                            }
                        }

                        onCurrentIndexChanged: {
                            settings.recorder_settings.dir_date = currentIndex === 0;
                            if (settings.recorder_settings.dir_date) {
                                dir.editText = settings.recorder_settings.dir_name;
                            } else {
                                settings.recorder_settings.dir_name = model.get(1).label;
                            }
                        }
                    }
                }
            }
        }
    }
}
