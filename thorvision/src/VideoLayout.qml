import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {

    StackLayout {
        anchors.fill: parent
        currentIndex: Theme.AppSettings.camera_detected ? Theme.AppSettings.selected_preview_index : 0

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {}
        }

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: gridLayout_1.implicitHeight + 50

                GridLayout {
                    id: gridLayout_1
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
                            camera_index: CameraModel.get(index).id
                        }
                    }
                }
            }
        }

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: gridLayout_4.implicitHeight + 250

                GridLayout {
                    id: gridLayout_4
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
                            camera_index: CameraModel.get(index).id
                        }
                    }
                }
            }
        }

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: gridLayout_6.implicitHeight + 300

                GridLayout {
                    id: gridLayout_6
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
                            camera_index: CameraModel.get(index).id
                        }
                    }
                }
            }
        }

        ScrollView {
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            Item {
                width: parent.width
                implicitHeight: gridLayout_12.implicitHeight + 180

                GridLayout {
                    id: gridLayout_12
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
                            camera_index: CameraModel.get(index).id
                        }
                    }
                }
            }
        }
    }
}
