pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: root

    property int layout_index: Theme.AppSettings.camera_detected ? Theme.AppSettings.selected_preview_index : 0

    // Loader {
    //     id: loader
    //     objectName: "video_loader"
    //     anchors.fill: parent
    //     sourceComponent: root.get_layout_component()
    // }

    // function get_layout_component() {
    //     switch (layout_index) {
    //     case 0:
    //         return no_camera;
    //     case 1:
    //         return layout_1;
    //     case 2:
    //         return layout_4;
    //     case 3:
    //         return layout_6;
    //     case 4:
    //         return layout_12;
    //     default:
    //         return no_camera;
    //     }
    // }

    // DelegateModel {
    //     id: delegate_model
    //     model: CameraModel
    //     objectName: "delegate_model"

    //     delegate: VideoPreview {
    //         id: video_preview
    //         objectName: "video_preview_" + index
    //     }
    // }

    ScrollView {
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        anchors.fill: parent

        Item {
            width: parent.width
            implicitHeight: grid_layout.implicitHeight + 100

            GridLayout {
                id: grid_layout
                columns: {
                    switch (root.layout_index) {
                    case 1:
                        return 1;   // layout 1
                    case 2:
                        return 2;   // layout 4 (2x2)
                    case 3:
                        return 3;   // layout 6 (3x2)
                    case 4:
                        return 4;   // layout 12 (4x3)
                    default:
                        return 0;   // no camera
                    }
                }
                rows: {
                    switch (root.layout_index) {
                    case 1:
                        return 1;
                    case 2:
                        return 2;
                    case 3:
                        return 2;
                    case 4:
                        return 3;
                    default:
                        return 0;
                    }
                }
                rowSpacing: 15
                columnSpacing: 15
                anchors.centerIn: parent

                Repeater {
                // Instantiator {
                    // id: repeater
                    // signal added(int index, Item item)
                    // signal removed(int index, Item item)
                    model: CameraModel
                    objectName: "repeater"

                    delegate: VideoPreview {
                        id: video_preview_1
                        Layout.preferredWidth: 400
                        Layout.preferredHeight: 300

                        required property int index

                        border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                        camera: CameraModel.get(index).camera_item
                        camera_id: CameraModel.get(index).id

                        // onWindowChanged: {
                        //     console.log("OnWindowChanged index:", index, video_preview_1);
                        //     repeater.added(index, video_preview_1);
                        // }
                        // Component.onCompleted: {
                        //     console.log("Created:", video_preview_1.objectName, "for camera index:", index);
                        //     console.log(video_preview_1.width, video_preview_1.height);
                        //     repeater.added(index, video_preview_1);
                        // }
                        // Component.onDestruction: {
                        //     console.log("Destroyed:", video_preview_1.objectName, "for camera index:", index);
                        //     console.log(video_preview_1.width, video_preview_1.height);
                        //     repeater.removed(index, video_preview_1);
                        // }
                    }

                    // onObjectAdded: {
                    //     console.log("Instantiator added index:", index, object);
                    // }
                    // onObjectRemoved: {
                    //     console.log("Instantiator removed index:", index, object);
                    // }
                }
            }
        }
    }

    Component {
        id: no_camera

        Item {
            id: container
            objectName: "container"

            GridLayout {
                id: grid_layout_0

                Instantiator {
                    // Repeater {
                    // id: repeater
                    model: CameraModel
                    objectName: "repeater"

                    delegate: VideoPreview {
                        id: video_preview
                        Layout.preferredWidth: 1152
                        Layout.preferredHeight: 864

                        objectName: "video_preview_" + index

                        required property int index

                        border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                        camera: CameraModel.get(index).camera_item
                        camera_id: CameraModel.get(index).id

                        Component.onCompleted: {
                            console.log("Created:", video_preview.objectName, "for camera index:", index);
                        }
                    }

                    Component.onCompleted: {
                        console.log("GRID LAYOUT", repeater.children);
                    }
                }

                Component.onCompleted: {
                    console.log("GRID LAYOUT", grid_layout_0.children);
                }
            }
        }
    }

    Component {
        id: layout_1

        // ScrollView {
        //     ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        //     ScrollBar.vertical.policy: ScrollBar.AlwaysOn

        Item {
            id: container
            width: parent.width
            // height: parent.height
            // implicitHeight: grid_layout_1.implicitHeight + 50
            objectName: "container"

            // GridView {
            //     id: grid
            //     model: CameraModel

            //     property int columns: 1
            //     property int rows: 1
            //     anchors.fill: parent
            //     cellWidth: container.width / columns - 200
            //     cellHeight: container.height / rows - 200
            //     focus: true
            //     objectName: "grid_1"

            //     delegate: VideoPreview {
            //         id: video_preview
            //         width: grid.cellWidth - 20
            //         height: grid.cellHeight - 20

            //         // objectName: "video_preview"
            //         objectName: "video_preview_" + index
            //         // Component.onCompleted: {
            //         //     video_preview.objectName = "video_preview_" + index;
            //         //     console.log("objectName: " + video_preview.objectName);
            //         // }

            //         required property int index

            //         border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

            //         camera: CameraModel.get(index).camera_item
            //         camera_id: CameraModel.get(index).id

            //         Component.onCompleted: {
            //             console.log("Created:", video_preview.objectName, "for camera index:", index);

            //         }
            //     }

            //     Component.onCompleted: {
            //         console.log("GridView columns:", grid.columns, "rows:", grid.rows, "cellWidth:", grid.cellWidth, "cellHeight:", grid.cellHeight);
            //     }
            // }

            GridLayout {
                id: grid_layout_1
                columns: 1
                rowSpacing: 15
                columnSpacing: 15

                anchors.centerIn: parent

                Repeater {
                    // id: repeater
                    model: CameraModel
                    objectName: "repeater"

                    delegate: VideoPreview {
                        id: video_preview
                        Layout.preferredWidth: 1152
                        Layout.preferredHeight: 864

                        // objectName: "video_preview_" + index
                        objectName: "video_preview"

                        required property int index

                        border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border

                        camera: CameraModel.get(index).camera_item
                        camera_id: CameraModel.get(index).id

                        Component.onCompleted: {
                            console.log("GRID LAYOUT", repeater.children);
                        }
                    }
                }
            }
        }
        // }
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
                        objectName: "repeater"

                        delegate: VideoPreview {
                            Layout.preferredWidth: 640
                            Layout.preferredHeight: 320

                            required property int index
                            objectName: "video_preview_" + index

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
