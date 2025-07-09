pragma Singleton

import QtQuick

QtObject {
    id: app_settings

    readonly property string app_name: qsTr("Thor Vision 1.0.0")

    property bool xdaq_connected: true

    property bool camera_detected: true
    property int camera_count: 0
    property int selected_camera_index: 0

    property bool drawer_visible: true

    property bool recording: false
    // TODO: Timer
}
