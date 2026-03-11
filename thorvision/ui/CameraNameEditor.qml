import QtQuick
import QtQuick.Layouts

import App.Theme 0.1 

Item {
    id: item

    property int index: -1
    property var camera: CameraModel.get(index).camera_item
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

                    visible: !editor.activeFocus
                    text: item.camera_name
                    font: AppFont.camera_settings_name
                    color: Color.text
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                    fontSizeMode: Text.Fit
                    maximumLineCount: 1
                    clip: true
                }

                TextInput {
                    id: editor

                    Layout.maximumWidth: 190

                    visible: activeFocus
                    text: item.camera_name
                    font: AppFont.camera_settings_name
                    color: Color.text
                    selectionColor: Color.accent
                    selectedTextColor: Color.text
                    clip: true

                    // TODO: limit length
                    maximumLength: 200
                    validator: RegularExpressionValidator {
                        regularExpression: /^[a-zA-Z0-9_() ]*/
                    }

                    onAccepted: {
                        item.camera.name = editor.text;
                        focus = false;
                    }

                    Keys.onEscapePressed: {
                        item.camera.name = editor.text;
                        focus = false;
                    }
                }

                Image {
                    id: icon

                    source: "qrc:/qt/qml/App/Theme/resources/change-name.svg"
                    sourceSize.width: 15
                    sourceSize.height: 15
                    fillMode: Image.PreserveAspectFit
                    visible: !editor.activeFocus
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
