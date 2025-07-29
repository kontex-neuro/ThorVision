import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: item

    property bool editing: false
    property var camera: null
    property string camera_name: camera ? camera.name : ""

    RowLayout {
        anchors.left: parent.left
        anchors.leftMargin: 12

        Item {
            Layout.preferredWidth: label.implicitWidth + icon.implicitWidth + 12
            Layout.preferredHeight: Math.max(label.implicitHeight, icon.implicitHeight)

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    id: label
                    visible: !item.editing
                    text: item.camera_name
                    font: Theme.Font.camera_settings_name
                    elide: Text.ElideRight
                    fontSizeMode: Text.Fit
                }

                TextInput {
                    id: editor

                    visible: item.editing
                    text: item.camera_name
                    font: Theme.Font.camera_settings_name
                    focus: item.editing
                    color: Theme.Color.text

                    onAccepted: {
                        console.log("onAccepted");

                        item.editing = false;
                        item.camera.set_name(editor.text);
                    }
                    onFocusChanged: {
                        console.log("onFocusChanged");

                        if (!focus) {
                            item.editing = false;
                            item.camera.set_name(editor.text);
                        }
                    }
                }

                Image {
                    id: icon
                    source: "qrc:/change-name.svg"
                    fillMode: Image.PreserveAspectFit
                    visible: !item.editing

                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton

                onClicked: {
                    item.editing = true;
                    editor.focus = true;
                    editor.selectAll();
                }
            }
        }
    }
}
