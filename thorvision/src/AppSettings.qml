pragma Singleton

import QtQuick

QtObject {
    readonly property string app_name: qsTr("ThorVision 1.0.1")

    readonly property bool xdaq_connected: Server.xdaq_connected

    readonly property bool camera_detected: xdaq_connected && CameraModel.rowCount > 0

    readonly property bool recording: Recorder.recording
    readonly property string recording_time: Recorder.recording_time

    property int selected_camera_index: CameraModel.selected_camera_index
    property int selected_preview_index: camera_detected ? 1 : 0

    // TODO
    property bool camera_settings_visible: true
}
