pragma Singleton

import QtQuick

QtObject {
    readonly property string app_name: qsTr("ThorVision 1.1.1")

    readonly property bool xdaq_connected: Server.xdaq_connected

    readonly property bool camera_detected: xdaq_connected && CameraModel.rowCount > 0

    readonly property bool recording: Recorder.recording
    readonly property string recording_time: Recorder.recording_time

    property int selected_camera_index: CameraModel.selected_camera_index
    property int selected_preview_index: 1

    readonly property bool all_cameras_streaming: CameraModel.all_cameras_streaming

    property bool camera_settings_visible: true

    readonly property string doc: "https://kontex-neuro.github.io/ThorVisionUserManual/"
    readonly property string license: "https://github.com/kontex-neuro/ThorVision/blob/qml/LICENSE"
    readonly property string report_issue: "https://github.com/kontex-neuro/ThorVision/issues"
}
