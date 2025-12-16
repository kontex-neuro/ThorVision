pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: root

    property int layout_index: Theme.AppSettings.selected_preview_index

    readonly property int columns: {
        switch (layout_index) {
        case 1:
            return 1; // layout 1 (1x1)
        case 2:
            return 2; // layout 4 (2x2)
        case 3:
            return 3; // layout 6 (3x2)
        case 4:
            return 4; // layout 12 (4x3)
        default:
            return 0; // no camera
        }
    }

    readonly property int rows: {
        switch (layout_index) {
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

    ScrollView {
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        anchors.fill: parent

        Item {
            width: parent.width
            implicitHeight: {
                switch (root.layout_index) {
                case 1:
                    return grid_layout.implicitHeight + 50;
                case 2:
                    return grid_layout.implicitHeight + 250;
                case 3:
                    return grid_layout.implicitHeight + 300;
                case 4:
                    return grid_layout.implicitHeight + 180;
                default:
                    return 0;
                }
            }

            GridLayout {
                id: grid_layout
                // width: parent.width
                // width: scrollView.availableWidth
                // height: scrollView.availableHeight

                rows: root.rows
                columns: root.columns
                rowSpacing: 15
                columnSpacing: 15

                anchors.centerIn: parent
                // anchors.margins: 15

                Repeater {
                    model: CameraModel
                    objectName: "repeater"

                    delegate: VideoPreview {
                        id: preview
                        Layout.preferredWidth: {
                            // const spacing = grid_layout.columnSpacing * (root.columns - 1);
                            // return (root.width - spacing) / root.columns;
                            switch (root.layout_index) {
                            case 1:
                                return 1152;
                            case 2:
                                return 640;
                            case 3:
                                return 400;
                            case 4:
                                return 320;
                            default:
                                return 0;
                            }
                        }
                        Layout.preferredHeight: {
                            // const spacing = grid_layout.rowSpacing * (root.rows - 1);
                            // return (root.height - spacing) / root.rows;
                            switch (root.layout_index) {
                            case 1:
                                return 864;
                            case 2:
                                return 320;
                            case 3:
                                return 300;
                            case 4:
                                return 240;
                            default:
                                return 0;
                            }
                        }

                        required property int index

                        camera: CameraModel.get(index).camera_item
                        selected_camera_index: index
                    }
                }
            }
        }
    }
}
