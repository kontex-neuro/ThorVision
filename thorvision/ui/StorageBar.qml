import QtQuick

import App.Theme 0.1

// SSD capacity as three segments: used | available for recording | reserved by the recorder.
// Colours name what the space is for, not how full the SSD is: a full SSD is normal for loop
// recording. Only a recorder that can't keep its reserve (Space fault) turns the reserve red.
Rectangle {
    id: bar

    readonly property real total: SsdStatus.total_bytes

    // Nonzero segments stay at least 2 px wide, so 30 GB used on 2 TB is still visible.
    function segment(bytes) {
        if (!total || bytes <= 0)
            return 0;
        return Math.max(2, (width - 2) * bytes / total);
    }

    color: Colour.storage_track
    border.color: SsdStatus.reserve_short ? Colour.status_critical : "#555555"
    border.width: 1
    radius: 3
    clip: true
    opacity: SsdStatus.capacity_last_known ? 0.55 : 1

    implicitHeight: 10

    Row {
        anchors.fill: parent
        anchors.margins: 1
        // 1 px track gap between segments: blue, green and amber are close in lightness.
        spacing: 1

        Rectangle {
            width: bar.segment(SsdStatus.used_bytes)
            height: parent.height
            color: Colour.storage_used
        }

        Rectangle {
            width: bar.segment(SsdStatus.available_bytes)
            height: parent.height
            color: Colour.storage_available
        }

        Rectangle {
            width: bar.segment(SsdStatus.reserve_bytes)
            height: parent.height
            color: SsdStatus.reserve_short ? Colour.status_critical : Colour.storage_reserved
        }
    }
}
