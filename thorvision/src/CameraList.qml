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
                font: Theme.Font.camera_title
                color: Theme.Color.text_1
            }
        }

        ListView {
            id: connected_camera_list

            model: CameraModel
            boundsBehavior: Flickable.StopAtBounds
            focus: true

            delegate: ItemDelegate {
                id: delegateRoot

                required property string name
                required property int index

                text: name
                width: parent.width - scroll_bar.width
                font: Theme.Font.camera_name

                highlighted: ListView.isCurrentItem

                background: Rectangle {
                    color: delegateRoot.highlighted ? Theme.Color.accent : "transparent"
                }

                onClicked: {
                    connected_camera_list.currentIndex = index;
                    Theme.AppSettings.selected_camera_index = index;
                    console.log("clicked:", name, "index:", index, " name:", CameraModel.name(index));
                }
            }

            ScrollBar.vertical: ScrollBar {
                id: scroll_bar
                policy: ScrollBar.AlwaysOn
            }
        }
    }
}
