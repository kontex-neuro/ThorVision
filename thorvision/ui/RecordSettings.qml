pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import App.Theme 0.1 

Rectangle {
    id: settings
    color: Color.spacer
    border.color: Color.spacer_border
    border.width: 1

    property var recorder_settings: RecorderSettings

    function commit_edit() {
        if (!dir.editable)
            return;

        dir.editing = false;
        dir.focus = false;

        let name = text_input.text.trim();
        if (name.length === 0 || !text_input.acceptableInput)
            name = qsTr("Untitled folder");

        settings.recorder_settings.dir_name = name;
    }

    GridLayout {
        anchors.fill: parent
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Item {
            enabled: !Recorder.recording

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
                    checked: settings.recorder_settings.split_on

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
                        font: AppFont.record_settings_text
                        color: enabled ? Color.text : Qt.darker(Color.text, 2)
                    }

                    CustomSpinBox {
                        enabled: split.checked

                        Layout.preferredWidth: 62

                        value: settings.recorder_settings.split_length
                        onValueModified: {
                            settings.recorder_settings.split_length = value;
                        }
                    }

                    CustomComboBox {
                        id: time_unit
                        model: [qsTr("Sec"), qsTr("Min"), qsTr("Hour"), qsTr("Day")]
                        enabled: split.checked

                        Layout.preferredWidth: 62

                        currentIndex: settings.recorder_settings.split_unit_index
                        onActivated: {
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
                    enabled: !Recorder.recording

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
                            enabled: !Recorder.recording

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

                    // TODO: ellipses if too long
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
                        enabled: !Recorder.recording
                        currentIndex: settings.recorder_settings.dir_date ? 0 : 1
                        displayText: settings.recorder_settings.dir_date ? "YYYY-MM-DD_HH-MM-SS" : settings.recorder_settings.dir_name
                        editText: settings.recorder_settings.dir_name

                        Layout.preferredWidth: 233

                        Connections {
                            target: settings.recorder_settings

                            function onDir_nameChanged() {
                                const name = settings.recorder_settings.dir_name;
                                if (name && name.length > 0) {
                                    // custom dir name is index 1
                                    dir_model.setProperty(1, "label", name);
                                }
                            }
                        }

                        property bool editing: false

                        delegate: ItemDelegate {
                            id: delegate

                            required property var model

                            width: dir.width
                            highlighted: ListView.isCurrentItem
                            background: Rectangle {
                                color: delegate.highlighted ? Color.accent : "transparent"
                            }
                            contentItem: Text {
                                text: (delegate.model.type === "Custom" ? "[Custom] " : "[Auto] ") + delegate.model.label
                                color: Color.text
                                font: AppFont.record_settings_dropdown
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }

                        background: Rectangle {
                            color: dir.enabled ? (dir.down ? Color.down_background : dir.editing ? Color.edit_background : (dir.hovered ? Color.hovered_background : Color.dropdown_background)) : Color.dropdown_background
                            border.color: dir.enabled ? (dir.editing ? Color.accent : (dir.hovered ? Color.hovered_border : Color.dropdown_border)) : Color.dropdown_border
                            border.width: 1
                        }

                        contentItem: Item {
                            Text {
                                id: text
                                text: dir.displayText
                                visible: !dir.editable
                                font: dir.font
                                color: Color.text
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

                                text: dir.editText
                                visible: dir.editable
                                font: dir.font
                                color: dir.editing ? Color.edit_text : Color.text
                                selectionColor: Color.accent
                                selectedTextColor: Color.text
                                clip: true

                                anchors.fill: parent
                                anchors.leftMargin: 5
                                anchors.rightMargin: 5
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                                // TODO: add ellipsis if too long
                                maximumLength: 30

                                validator: RegularExpressionValidator {
                                    regularExpression: /^[a-zA-Z0-9_ ]+$/
                                }

                                onEditingFinished: {
                                    settings.commit_edit();
                                }
                                onActiveFocusChanged: {
                                    dir.editing = activeFocus;
                                    if (!activeFocus) {
                                        settings.commit_edit();
                                    }
                                }
                            }
                        }

                        onActivated: {
                            const is_auto = currentIndex === 0;
                            settings.recorder_settings.dir_date = is_auto;
                        }
                    }
                }
            }
        }
    }
}
