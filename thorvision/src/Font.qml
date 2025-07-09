pragma Singleton

import QtQuick

QtObject {
    id: font

    readonly property string helvetica: "Helvetica"

    // Camera Block
    readonly property var camera_title: ({
            family: helvetica,
            pointSize: 24,
            weight: Font.Normal
        })
    readonly property var camera_name: ({
            family: helvetica,
            pointSize: 15,
            weight: Font.Normal
        })
    readonly property var camera_count: ({
            family: helvetica,
            pointSize: 20
        })

    // Record Settings
    readonly property var record_setting_label: ({
            family: helvetica,
            pointSize: 15
        })
    readonly property var record_path_field: ({
            family: helvetica,
            pointSize: 13
        })
    readonly property var dropdown_field: ({
            family: helvetica,
            pointSize: 12
        })
    readonly property var hover_hint: ({
            family: helvetica,
            pointSize: 7
        })

    // Record Buttons
    readonly property var record: ({
            family: helvetica,
            pointSize: 20
        })

    // XDAQ status
    readonly property var xdaq_status: ({
            family: helvetica,
            pointSize: 13
        })

    // Camera Settings
    readonly property var camera_setting_title: ({
            family: helvetica,
            pointSize: 18
        })
    readonly property var camera_setting_label: ({
            family: helvetica,
            pointSize: 15
        })
    readonly property var camera_option_field: ({
            family: helvetica,
            pointSize: 13
        })
    readonly property var camera_dropdown: ({
            family: helvetica,
            pointSize: 12
        })
    readonly property var camera_setting_info: ({
            family: helvetica,
            pointSize: 10
        })

    // Popup Window
    readonly property var popupTitle: ({
            family: helvetica,
            pointSize: 18
        })
    readonly property var popupNormalText: ({
            family: helvetica,
            pointSize: 18,
            lineHeight: 30
        })
    readonly property var popupScrollText: ({
            family: helvetica,
            pointSize: 13,
            lineHeight: 20
        })
}
