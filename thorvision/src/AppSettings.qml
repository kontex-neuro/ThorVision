pragma Singleton

import QtQuick

QtObject {
    readonly property string app_name: qsTr("Thor Vision 1.0.0")

    property bool xdaq_connected: true

    property bool camera_detected: true
    property int selected_camera_index: 0
    property int selected_preview_index: camera_detected ? 1 : 0

    property bool camera_settings_visible: true

    property bool recording: false
    // TODO: Timer
}
