pragma Singleton

import QtQuick

QtObject {
    id: color

    property bool dark_mode: Application.styleHints.colorScheme === Qt.ColorScheme.Dark

    // Background
    readonly property color color_background_dark_1: "#1a1a1a"
    readonly property color color_background_dark_2: "#333333"
    readonly property color color_background_dark_3: "#3a3a3a"
    readonly property color color_background_dark_4: "#282828"

    // Neutral
    readonly property color color_gray_border_1: "#666666"
    readonly property color color_gray_border_2: "#4d4d4d"
    readonly property color color_gray_light: "#b3b3b3"
    readonly property color color_gray_lighter: "#f2f2f2"

    // Accent
    readonly property color accent: "#928263"
    readonly property color color_accent_2: "#992029"
}
