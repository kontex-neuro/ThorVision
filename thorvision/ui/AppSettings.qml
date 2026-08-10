pragma Singleton

import QtQuick

QtObject {
    readonly property string app_name: qsTr("ThorVision 1.3.0")

    readonly property bool xdaq_connected: Server.xdaq_connected
    readonly property int camera_count: CameraModel.rowCount
    readonly property bool camera_detected: xdaq_connected && camera_count > 0
    
    readonly property string doc: "https://kontex-neuro.github.io/ThorVisionUserManual/"
    readonly property string report_issue: "https://github.com/kontex-neuro/ThorVision/issues"

    property int selected_preview_index: 1
    property bool camera_settings_visible: true
}
