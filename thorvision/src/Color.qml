pragma Singleton

import QtQuick

QtObject {
    property bool dark_mode: Application.styleHints.colorScheme === Qt.ColorScheme.Dark

    readonly property color camera_list: dark_mode ? "#333333" : "#dfdfdf"
    readonly property color camera_list_border: dark_mode ? "#808080" : "#b2b2b2"

    readonly property color spacer: dark_mode ? "#4d4d4d" : "#ebebeb"
    readonly property color spacer_border: dark_mode ? "#808080" : "#b2b2b2"

    readonly property color record_settings: dark_mode ? "#4d4d4d" : "#ebebeb"
    readonly property color record_settings_border: dark_mode ? "#808080" : "#b2b2b2"

    readonly property color record: dark_mode ? "#333333" : "#dfdfdf"
    readonly property color record_border: dark_mode ? "#808080" : "#b2b2b2"

    readonly property color xdaq_status: dark_mode ? "#4d4d4d" : "#ebebeb"
    readonly property color xdaq_status_border: dark_mode ? "#808080" : "#b2b2b2"

    readonly property color top_spacer: dark_mode ? "#232323" : "#f2f2f2"
    readonly property color top_spacer_border: dark_mode ? "#1a1a1a" : "#b2b2b2"

    readonly property color preview: dark_mode ? "#333333" : "#ebebeb"
    // readonly property color preview_button: dark_mode ? "#666666" : "#b2b2b2"

    readonly property color video_layout: dark_mode ? "#333333" : "#ebebeb"

    readonly property color video: "#1a1a1a"
    // readonly property color video_border: dark_mode ? "#808080" : "#b2b2b2"
    readonly property color video_border: "#666666"

    readonly property color camera_settings: dark_mode ? "#4d4d4d" : "#ebebeb"
    readonly property color camera_settings_border: dark_mode ? "#1a1a1a" : "#ffffff"

    readonly property color text_1: "#f1f1f1"
    readonly property color text_2: "#cbcbcb"

    readonly property color accent: dark_mode ? "#928263" : "#fbb03b"
    readonly property color info: "#94805f"
    readonly property color warn: "#a60523"
}
