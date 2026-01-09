pragma Singleton

import QtQuick

QtObject {
    readonly property color background_1: "#333333"
    readonly property color background_2: "#4d4d4d"
    readonly property color background_3: "#232323"
    readonly property color background_4: "#666666"
    readonly property color background_5: "#f1f1f1"
    readonly property color background_6: "#ffffff"

    readonly property color border_1: "#808080"
    readonly property color border_2: "#1a1a1a"
    readonly property color border_3: "#b2b2b2"
    readonly property color border_4: "#999999"
    readonly property color border_5: "#000000"

    readonly property color text: "#f1f1f1"
    readonly property color accent: "#928263"
    readonly property color warn: "#a60523"

    // top row
    readonly property color camera_list: background_1
    readonly property color camera_list_border: border_1
    readonly property color spacer: background_2
    readonly property color spacer_border: border_1
    readonly property color record_settings: background_2
    readonly property color record_settings_border: border_1
    readonly property color record: background_1
    readonly property color record_border: border_1
    readonly property color xdaq_status: background_2
    readonly property color xdaq_status_border: border_1

    // top spacer
    readonly property color top_spacer: background_3
    readonly property color top_spacer_border: border_2

    // video layout
    readonly property color preview: background_1
    readonly property color video_layout: background_1
    readonly property color video: border_2
    readonly property color video_border: border_1
    readonly property color camera_settings: background_2
    readonly property color camera_settings_border: border_2

    // custom
    readonly property color popup_header: border_2
    readonly property color popup_background: background_2
    readonly property color popup_border: border_3
    readonly property color popup_button: background_4
    readonly property color dropdown_background: background_1
    readonly property color dropdown_border: background_4
    readonly property color checkbox_background: background_1
    readonly property color checkbox_border: border_5
    readonly property color checkbox_checked: border_3
    readonly property color hovered_checkbox_border: background_6
    readonly property color spinbox_background: background_1
    readonly property color spinbox_border: background_4
    readonly property color record_button_background: background_1
    readonly property color record_button_border: background_4
    readonly property color hovered_border: border_4
    readonly property color hovered_background: background_2
    readonly property color down_background: border_2
    readonly property color edit_background: background_6
    readonly property color edit_text: border_5
}
