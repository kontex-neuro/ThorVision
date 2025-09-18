import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: root

    property int layout_index: Theme.AppSettings.camera_detected ? Theme.AppSettings.selected_preview_index : 0

    Loader {
        anchors.fill: parent
        sourceComponent: root.get_layout_component()
    }

    function get_layout_component() {
        switch (layout_index) {
        case 0:
            return no_camera;
        case 1:
            return layout_1;
        case 2:
            return layout_4;
        case 3:
            return layout_6;
        case 4:
            return layout_12;
        default:
            return no_camera;
        }
    }

    Component {
        id: no_camera

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {}
        }
    }

    Component {
        id: layout_1

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: grid_layout_1.implicitHeight + 50

                GridLayout {
                    id: grid_layout_1
                    columns: 1
                    rowSpacing: 15
                    columnSpacing: 15

                    anchors.centerIn: parent

                    Repeater {
                        model: CameraModel

                        delegate: VideoPreview {
                            Layout.preferredWidth: 1152
                            Layout.preferredHeight: 864

                            required property int index

                            border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                            camera: CameraModel.get(index).camera_item
                            camera_id: CameraModel.get(index).id
                        }
                    }
                }
            }
        }
    }

    Component {
        id: layout_4

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: grid_layout_4.implicitHeight + 250

                GridLayout {
                    id: grid_layout_4
                    columns: 2
                    rows: 2
                    rowSpacing: 15
                    columnSpacing: 15

                    anchors.centerIn: parent

                    Repeater {
                        model: CameraModel

                        delegate: VideoPreview {
                            Layout.preferredWidth: 640
                            Layout.preferredHeight: 320

                            required property int index

                            border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                            camera: CameraModel.get(index).camera_item
                            camera_id: CameraModel.get(index).id
                        }
                    }
                }
            }
        }
    }

    Component {
        id: layout_6

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: grid_layout_6.implicitHeight + 300

                GridLayout {
                    id: grid_layout_6
                    columns: 3
                    rows: 2
                    rowSpacing: 15
                    columnSpacing: 15

                    anchors.centerIn: parent

                    Repeater {
                        model: CameraModel

                        delegate: VideoPreview {
                            Layout.preferredWidth: 400
                            Layout.preferredHeight: 300

                            required property int index

                            border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                            camera: CameraModel.get(index).camera_item
                            camera_id: CameraModel.get(index).id
                        }
                    }
                }
            }
        }
    }

    Component {
        id: layout_12

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: grid_layout_12.implicitHeight + 180

                GridLayout {
                    id: grid_layout_12
                    columns: 4
                    rows: 3
                    rowSpacing: 15
                    columnSpacing: 15

                    anchors.centerIn: parent

                    Repeater {
                        model: CameraModel

                        delegate: VideoPreview {
                            Layout.preferredWidth: 320
                            Layout.preferredHeight: 240

                            required property int index

                            border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                            camera: CameraModel.get(index).camera_item
                            camera_id: CameraModel.get(index).id
                        }
                    }
                }
            }
        }
    }
}
