pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: root

    property int layout_index: Theme.AppSettings.selected_preview_index

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
                    model: CameraModel
                    objectName: "repeater"

                    delegate: VideoPreview {
                        id: preview
                        Layout.preferredWidth: {
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

                        border.color: (index === Theme.AppSettings.selected_camera_index) ? Theme.Color.accent : Theme.Color.video_border
                        camera: CameraModel.get(index).camera_item

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                CameraModel.set_selected_camera_index(preview.index);
                            }
                        }
                    }
                }
            }
        }
    }
}
