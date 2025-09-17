pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: camera_list

    property int index: Theme.AppSettings.camera_detected ? 1 : 0
    property int camera_count: connected_camera_list.count

    StackLayout {
        anchors.fill: parent
        currentIndex: camera_list.index

        Item {
            Label {
                text: qsTr("No Camera Found")
                anchors.centerIn: parent
                font: Theme.Font.no_camera_found
                color: Theme.Color.text
            }
        }

        ListView {
            id: connected_camera_list

            model: CameraModel
            boundsBehavior: Flickable.StopAtBounds
            focus: true

            ScrollBar.vertical: ScrollBar {
                id: scroll_bar
                policy: ScrollBar.AlwaysOn
            }

            delegate: ItemDelegate {
                id: delegate

                required property int index
                property var camera: CameraModel.get(index).camera_item
                property string camera_name: camera ? camera.name : ""

                width: connected_camera_list.width - scroll_bar.width
                leftPadding: 12

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
                    connected_camera_list.currentIndex = index;
                    CameraModel.set_selected_camera_index(index);
                    console.log("Selected:", delegate.camera_name, "index:", index);
                }
            }
        }
    }
}
