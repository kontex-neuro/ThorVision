pragma Singleton

import QtQuick

QtObject {
    readonly property string helvetica: "Helvetica"

    // Camera Block
    readonly property font camera_title: ({
            family: helvetica,
            pointSize: 24,
            weight: Font.Normal
        })
    readonly property font camera_name: ({
            family: helvetica,
            pointSize: 15,
            weight: Font.Light,
            letterSpacing: 0
        })
    readonly property font camera_count: ({
            family: helvetica,
            pointSize: 20
        })

    // Record Settings
    readonly property font record_setting_label: ({
            family: helvetica,
            pointSize: 15,
            weight: Font.Light,
            letterSpacing: 0
        })
    readonly property font record_settings_options: ({
            family: helvetica,
            pointSize: 13,
            weight: Font.Light,
            letterSpacing: 0.33
        })
    readonly property font hover_hint: ({
            family: helvetica,
            pointSize: 7
        })

    // Record Buttons
    readonly property font record: ({
            family: helvetica,
            pointSize: 20
        })

    // XDAQ status
    readonly property font xdaq_status: ({
            family: helvetica,
            pointSize: 13
        })

    // Camera Settings
    readonly property font camera_setting_title: ({
            family: helvetica,
            pointSize: 18,
            letterSpacing: 1.35
        })
    readonly property font camera_setting_label: ({
            family: helvetica,
            pointSize: 15,
            weight: Font.Light,
            letterSpacing: 0
        })
    readonly property font camera_option_field: ({
            family: helvetica,
            pointSize: 13,
            weight: Font.Light,
            letterSpacing: 0
        })
    readonly property font camera_dropdown: ({
            family: helvetica,
            pointSize: 12
        })
    readonly property font camera_setting_info: ({
            family: helvetica,
            pointSize: 10,
            weight: Font.Light,
            letterSpacing: 1.35
        })

    // Popup Window
    readonly property font popupTitle: ({
            family: helvetica,
            pointSize: 18
        })
    readonly property font popupNormalText: ({
            family: helvetica,
            pointSize: 18,
            lineHeight: 30
        })
    readonly property font popupScrollText: ({
            family: helvetica,
            pointSize: 13,
            lineHeight: 20
        })
}
