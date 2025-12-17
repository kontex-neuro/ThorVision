import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: item

    property var camera: null
    property string camera_name: camera ? camera.name : ""

    RowLayout {
        anchors.left: parent.left
        anchors.leftMargin: 12

        Item {
            Layout.preferredWidth: Math.min(label.implicitWidth + icon.implicitWidth + 12, 200)
            Layout.preferredHeight: Math.max(label.implicitHeight, icon.implicitHeight)

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Text {
                    id: label

                    Layout.maximumWidth: 190
                    clip: true
                    visible: !editor.activeFocus
                    text: item.camera_name
                    font: Theme.Font.camera_settings_name
                    color: Theme.Color.text
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                    fontSizeMode: Text.Fit
                    maximumLineCount: 1
                }

                TextInput {
                    id: editor

                    Layout.maximumWidth: 190
                    clip: true
                    visible: activeFocus
                    text: item.camera_name
                    font: Theme.Font.camera_settings_name
                    color: Theme.Color.text
                    selectionColor: Theme.Color.accent
                    selectedTextColor: Theme.Color.text

                    maximumLength: 20
                    validator: RegularExpressionValidator {
                        regularExpression: /^[a-zA-Z0-9_ ]*/
                    }

                    onAccepted: {
                        focus = false;
                    }
                    onActiveFocusChanged: {
                        console.log("onActiveFocusChanged", activeFocus);
                        if (!activeFocus) {
                            item.camera.set_name(editor.text);
                        }
                    }

                    Keys.onEscapePressed: {
                        focus = false;
                    }
                }

                Image {
                    id: icon

                    source: "qrc:/qt/qml/App/Theme/resources/change-name.svg"
                    fillMode: Image.PreserveAspectFit
                    visible: !editor.activeFocus

                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton

                onClicked: {
                    editor.forceActiveFocus(Qt.MouseFocusReason);
                    editor.selectAll();
                }
            }
        }
    }
}
