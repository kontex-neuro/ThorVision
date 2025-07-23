pragma Singleton

import QtQuick

QtObject {
    readonly property string app_name: qsTr("Thor Vision 1.0.0")

    property bool xdaq_connected: true

    property bool camera_detected: CameraModel.count > 0
    property int selected_camera_index: CameraModel.selected_camera_index
    property int selected_preview_index: camera_detected ? 1 : 0

    property bool camera_settings_visible: true

    property bool recording: false
    property int recording_time: 0
    readonly property string formatted_recording_time: {
        let hrs = Math.floor(recording_time / 3600).toString().padStart(2, '0');
        let mins = Math.floor((recording_time % 3600) / 60).toString().padStart(2, '0');
        let secs = (recording_time % 60).toString().padStart(2, '0');
        return `${hrs}:${mins}:${secs}`;
    }
}
