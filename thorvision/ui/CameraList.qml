pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
    color: Color.camera_list
    border.color: Color.camera_list_border
    border.width: 1

    Connections {
        target: CameraModel
        function onSelected_camera_changed(index) {
            list_view.currentIndex = index;
            list_view.forceActiveFocus(Qt.OtherFocusReason);
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: AppSettings.camera_detected ? 1 : 0

        Item {
            Label {
                text: qsTr("No Camera Found")
                anchors.centerIn: parent
                font: AppFont.no_camera_found
                color: Color.text
            }
        }

        ListView {
            id: list_view
            model: CameraModel
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                id: scroll_bar
                policy: ScrollBar.AlwaysOn
            }

            delegate: ItemDelegate {
                id: delegate
                width: list_view.width - scroll_bar.width
                leftPadding: 12

                required property int index
                property var camera: CameraModel.get(index).camera_item
                property string camera_name: camera ? camera.name : ""

                highlighted: ListView.isCurrentItem
                background: Rectangle {
                    color: delegate.highlighted ? Color.accent : "transparent"
                }
                contentItem: Text {
                    text: delegate.camera_name
                    color: Color.text
                    font: AppFont.camera_name
                }

                onClicked: {
                    CameraModel.selected_camera_index = index;
                }
            }

            Keys.onPressed: event => {
                switch (event.key) {
                case Qt.Key_Up:
                    if (list_view.currentIndex > 0) {
                        list_view.currentIndex -= 1;
                        CameraModel.selected_camera_index = list_view.currentIndex;
                        event.accepted = true;
                    }
                    break;
                case Qt.Key_Down:
                    if (list_view.currentIndex < AppSettings.camera_count - 1) {
                        list_view.currentIndex += 1;
                        CameraModel.selected_camera_index = list_view.currentIndex;
                        event.accepted = true;
                    }
                    break;
                }
            }
        }
    }
}
