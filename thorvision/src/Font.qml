pragma Singleton

import QtQuick

QtObject {
    readonly property string helvetica: "Helvetica"

    // Camera Block
    readonly property font no_camera_found: ({
            family: helvetica,
            weight: Font.Normal,
            pixelSize: 24,
            letterSpacing: 1.8
        })
    readonly property font camera_name: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 15,
            letterSpacing: 1.13
        })
    readonly property font camera_count: ({
            family: helvetica,
            weight: Font.Normal,
            pixelSize: 20,
            letterSpacing: 2
        })

    // Record Settings
    readonly property font record_settings_text: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 15,
            letterSpacing: 1.13
        })
    readonly property font record_settings_dropdown: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 13,
            letterSpacing: 0.33
        })
    readonly property font hover_hint: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 7,
            letterSpacing: 0.17
        })

    // Record Buttons
    readonly property font record: ({
            family: helvetica,
            weight: Font.Normal,
            pixelSize: 20,
            letterSpacing: 2
        })

    // XDAQ status
    readonly property font xdaq_status: ({
            family: helvetica,
            weight: Font.Normal,
            pixelSize: 13,
            letterSpacing: 0.97
        })

    // Camera Settings
    readonly property font camera_settings_name: ({
            family: helvetica,
            weight: Font.Normal,
            pixelSize: 18,
            letterSpacing: 1.35
        })
    readonly property font camera_settings_text: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 15,
            letterSpacing: 1.13
        })
    readonly property font camera_settings_dropdown: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 13,
            letterSpacing: 0.33
        })
    readonly property font camera_settings_info: ({
            family: helvetica,
            pixelSize: 12,
            weight: Font.Light,
            letterSpacing: 0.4
        })

    // Popup Window
    readonly property font popup_text: ({
            family: helvetica,
            weight: Font.Normal,
            pixelSize: 18,
            letterSpacing: 1.35
        })
    readonly property font popup_scroll_text: ({
            family: helvetica,
            weight: Font.Light,
            pixelSize: 15,
            letterSpacing: 1.13
        })
}
