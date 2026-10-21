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
    readonly property color accent1: "#036EB8"

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
    readonly property color status_bar_background: accent1

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

    // update
    readonly property color success: "#2f9e5f"
    readonly property color progress_track: background_1
    readonly property color progress_fill: accent1
    readonly property color update_badge: accent
    readonly property color update_badge_required: warn
    readonly property color log_background: background_3
    readonly property color log_text: "#c8c8c8"

    // status -- text/icon colours, each >= 4.5:1 on background_1. warn and success are
    // too dark to read as text on the greys (1.6:1 and 3.7:1), so status uses these.
    // Always pair a status colour with a StatusIcon shape and a word, never colour alone.
    readonly property color status_ok: "#5cc98d"
    readonly property color status_caution: "#f0c36a"
    readonly property color status_critical: "#ff8080"
    readonly property color status_info: "#6cb4f0"
    readonly property color status_unknown: "#9a9a9a"
    readonly property color text_muted: "#b8b8b8"

    // fills that sit under text
    readonly property color accent_fill: "#6b5e45"
    readonly property color critical_fill: "#8a1c2e"
    readonly property color caution_panel: "#3a3222"
    readonly property color caution_panel_border: "#6e5a2c"
    readonly property color critical_panel: "#3b2326"
    readonly property color critical_panel_border: "#7a3a42"
    readonly property color info_panel: "#22303b"
    readonly property color info_panel_border: "#36556e"
    readonly property color storage_used: "#8f8f8f"
    readonly property color storage_reserved: "#4a4a4a"
    readonly property color storage_track: "#2a2a2a"
}
