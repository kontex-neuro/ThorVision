pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: camera_list

    Connections {
        target: Bus
        function onCamera_selected(index) {
            CameraModel.set_selected_camera_index(index);
            list_view.currentIndex = index;
            list_view.forceActiveFocus(Qt.OtherFocusReason);
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

        Item {
            Label {
                text: qsTr("No Camera Found")
                anchors.centerIn: parent
                font: Theme.Font.no_camera_found
                color: Theme.Color.text
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
                    color: delegate.highlighted ? Theme.Color.accent : "transparent"
                }
                contentItem: Text {
                    text: delegate.camera_name
                    color: Theme.Color.text
                    font: Theme.Font.camera_name
                }

                onClicked: {
                    Bus.camera_selected(index);
                }
            }

            Keys.onPressed: event => {
                switch (event.key) {
                case Qt.Key_Up:
                    if (list_view.currentIndex > 0) {
                        list_view.currentIndex -= 1;
                        Bus.camera_selected(list_view.currentIndex);
                        event.accepted = true;
                    }
                    break;
                case Qt.Key_Down:
                    if (list_view.currentIndex < CameraModel.rowCount - 1) {
                        list_view.currentIndex += 1;
                        Bus.camera_selected(list_view.currentIndex);
                        event.accepted = true;
                    }
                    break;
                }
            }
        }
    }
}
