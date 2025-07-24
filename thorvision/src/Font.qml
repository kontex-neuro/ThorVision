pragma Singleton

import QtQuick

QtObject {
    readonly property string helvetica: "Helvetica"

    // Camera Block
    readonly property font camera_title: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 24
        })
    readonly property font camera_name: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 15,
            letterSpacing: 0
            // lineHeight: 20
        })
    readonly property font camera_count: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 20
        })

    // Record Settings
    readonly property font record_setting_label: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 15,
            letterSpacing: 0
        })
    readonly property font record_settings_options_selected: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 13,
            letterSpacing: 0.33
        })
    readonly property font record_settings_options: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 12,
            letterSpacing: 0.33
        })
    readonly property font hover_hint: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 7
        })

    // Record Buttons
    readonly property font record: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 20
        })

    // XDAQ status
    readonly property font xdaq_status: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 13
        })

    // Camera Settings
    readonly property font camera_setting_title: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 18,
            letterSpacing: 1.35
        })
    readonly property font camera_setting_label: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 15,
            letterSpacing: 0
        })
    readonly property font camera_option_field: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 13,
            letterSpacing: 0
        })
    readonly property font camera_dropdown: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 12
        })
    readonly property font camera_setting_info: ({
            family: helvetica,
            pointSize: 12,
            weight: Font.Light,
            letterSpacing: 1.35
        })

    // Popup Window
    readonly property font popup_title: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 18,
            letterSpacing: 1.35
        })
    readonly property font popup_normal_text: ({
            family: helvetica,
            weight: Font.Normal,
            pointSize: 18,
            letterSpacing: 1.35,
        })
    readonly property font popup_scroll_text: ({
            family: helvetica,
            weight: Font.Light,
            pointSize: 15,
            letterSpacing: 1.13
        })
}
