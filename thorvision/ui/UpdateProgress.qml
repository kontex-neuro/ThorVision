import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// Progress body for the in-flight phases: a phase label, one bar, the do-not-disconnect
// warning, and a disclosure that reveals the device log.
ColumnLayout {
    id: root

    property bool details_visible: false

    spacing: 14

    Text {
        text: qsTr("Installing update to %1").arg(Update.target_version)
        font: AppFont.popup_text
        color: Colour.text

        Layout.fillWidth: true
    }

    Text {
        text: qsTr("Step %1 of %2 — %3").arg(Update.phase_index).arg(Update.phase_count).arg(Update.phase_text)
        font: AppFont.popup_scroll_text
        color: Colour.text
        opacity: 0.8

        Layout.fillWidth: true
    }

    // The install phase is a single opaque call that runs for roughly a minute with no
    // progress to report. Without a ticking clock an indeterminate bar reads as a hang.
    Text {
        id: elapsed_label

        property int seconds: 0

        text: qsTr("This can take a minute or two. Elapsed: %1:%2").arg(Math.floor(seconds / 60)).arg(String(seconds % 60).padStart(2, "0"))
        font: AppFont.popup_scroll_text
        color: Colour.text
        opacity: 0.65
        visible: Update.progress_indeterminate

        Layout.fillWidth: true

        Timer {
            interval: 1000
            repeat: true
            running: Update.in_progress

            onTriggered: elapsed_label.seconds++
        }

        Connections {
            target: Update
            // Restart the count at each phase change so it measures the current step.
            function onState_changed() {
                elapsed_label.seconds = 0;
            }
        }
    }

    RowLayout {
        spacing: 12

        Layout.fillWidth: true

        ProgressBar {
            id: bar

            // `total` is 0 when the device sends no Content-Length; the controller reports
            // that as indeterminate rather than dividing by zero.
            indeterminate: Update.progress_indeterminate
            value: Update.progress
            from: 0
            to: 1

            Layout.fillWidth: true
            Layout.preferredHeight: 8

            background: Rectangle {
                implicitHeight: 8
                color: Colour.progress_track
                radius: 4
            }

            contentItem: Item {
                implicitHeight: 8
                clip: true

                Rectangle {
                    id: fill

                    width: bar.indeterminate ? parent.width * 0.3 : bar.visualPosition * parent.width
                    height: parent.height
                    radius: 4
                    color: Colour.progress_fill

                    // The sweep leaves x wherever it stopped, and an animation declared with
                    // `on x` keeps ownership of the property, so a plain binding will not
                    // reassert. Reset explicitly when the phase changes -- otherwise the
                    // determinate fill starts partway across the track instead of at 0.
                    Connections {
                        target: bar
                        function onIndeterminateChanged() {
                            if (!bar.indeterminate)
                                fill.x = 0;
                        }
                    }

                    SequentialAnimation on x {
                        running: bar.indeterminate && bar.visible
                        loops: Animation.Infinite

                        NumberAnimation {
                            from: 0
                            to: bar.width * 0.7
                            duration: 1100
                            easing.type: Easing.InOutQuad
                        }
                        NumberAnimation {
                            from: bar.width * 0.7
                            to: 0
                            duration: 1100
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }
        }

        Text {
            text: Update.progress_indeterminate ? "" : Math.round(Update.progress * 100) + "%"
            font: AppFont.popup_scroll_text
            color: Colour.text
            visible: !Update.progress_indeterminate

            Layout.preferredWidth: 44
        }
    }

    RowLayout {
        spacing: 8

        Layout.fillWidth: true

        Image {
            source: "qrc:/qt/qml/App/Theme/resources/warning.svg"
            sourceSize.width: 15
            sourceSize.height: 14
            fillMode: Image.PreserveAspectFit
        }

        Text {
            text: qsTr("Do not disconnect the device or close this window.")
            font: AppFont.popup_scroll_text
            color: Colour.text

            Layout.fillWidth: true
        }
    }

    Item {
        implicitHeight: disclosure.implicitHeight

        Layout.fillWidth: true

        Text {
            id: disclosure

            text: (root.details_visible ? "▾ " : "▸ ") + qsTr("Show details")
            font: AppFont.popup_scroll_text
            color: Colour.text
            opacity: mouse.containsMouse ? 1.0 : 0.75

            MouseArea {
                id: mouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: root.details_visible = !root.details_visible
            }
        }
    }

    UpdateLogView {
        model: Update.logs
        visible: root.details_visible

        Layout.fillWidth: true
        Layout.preferredHeight: visible ? 150 : 0
    }

    Item {
        Layout.fillHeight: true
    }
}
