pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
    id: root
    color: Colour.video_layout

    Connections {
        target: CameraModel
        function onSelected_camera_changed(index) {
            const item = repeater.itemAt(index);
            if (!item)
                return;
                
            item.center_preview();
        }
    }

    property int layout_index: AppSettings.selected_preview_index

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

    readonly property real aspect_ratio: 4 / 3
    readonly property int min_width: 320
    readonly property int min_height: min_width / aspect_ratio
    readonly property int max_width: 1152
    readonly property int max_height: max_width / aspect_ratio

    readonly property real h_padding: 32
    readonly property real v_padding: 42

    readonly property real available_width: width - h_padding
    readonly property real available_height: height - v_padding
    readonly property real spacing_w: grid_layout.columnSpacing * (columns - 1)
    readonly property real spacing_h: grid_layout.rowSpacing * (rows - 1)

    readonly property real cell_width: columns > 0 ? (available_width - spacing_w) / columns : 0
    readonly property real cell_height: rows > 0 ? (available_height - spacing_h) / rows : 0

    readonly property real preview_width: Math.max(min_width, Math.min(max_width, cell_width))
    readonly property real preview_height: preview_width / aspect_ratio

    ScrollView {
        id: scrollView

        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        anchors.fill: parent
        contentWidth: grid_layout.implicitWidth + root.h_padding
        contentHeight: grid_layout.implicitHeight + root.v_padding

        Item {
            width: Math.max(scrollView.width, grid_layout.implicitWidth + root.h_padding)
            implicitHeight: Math.max(scrollView.height, grid_layout.implicitHeight + root.v_padding)

            GridLayout {
                id: grid_layout

                rows: root.rows
                columns: root.columns
                rowSpacing: 15
                columnSpacing: rowSpacing

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                    horizontalCenterOffset: -root.h_padding / 2

                    // leftMargin: root.h_padding / 2
                    // rightMargin: root.h_padding / 2
                    topMargin: root.v_padding / 2
                    bottomMargin: root.v_padding / 2
                }

                Repeater {
                    id: repeater
                    model: CameraModel
                    objectName: "repeater"

                    delegate: VideoPreview {
                        id: preview

                        Layout.preferredWidth: root.preview_width
                        Layout.preferredHeight: root.preview_height
                        Layout.minimumWidth: root.min_width
                        Layout.minimumHeight: root.min_height
                        Layout.maximumWidth: root.max_width
                        Layout.maximumHeight: root.max_height

                        required property int index
                        selected_camera_index: index

                        function center_preview() {
                            const flickable = scrollView.contentItem;
                            const center = preview.mapToItem(flickable.contentItem, preview.width / 2, preview.height / 2);

                            let x = center.x - flickable.width / 2;
                            let y = center.y - flickable.height / 2;

                            x = Math.max(0, Math.min(x, flickable.contentWidth - flickable.width));
                            y = Math.max(0, Math.min(y, flickable.contentHeight - flickable.height));

                            animation.content_x = x;
                            animation.content_y = y;
                            animation.restart();
                        }
                    }
                }
            }
        }
    }

    ParallelAnimation {
        id: animation

        property real content_x: 0
        property real content_y: 0

        NumberAnimation {
            target: scrollView.contentItem
            property: "contentX"
            to: animation.content_x
            duration: 300
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: scrollView.contentItem
            property: "contentY"
            to: animation.content_y
            duration: 300
            easing.type: Easing.OutCubic
        }
    }
}
