import QtQuick
import QtQuick.Controls

import App.Theme 0.1

// Device-side update log. Revealed by the "Show details" disclosure on UpdateProgress --
// the field failure modes for the device transfer are near-impossible to diagnose from a
// progress bar alone.
Rectangle {
    id: root

    property alias model: list.model

    color: Colour.log_background
    border.color: Colour.popup_border
    border.width: 1
    radius: 2
    clip: true

    ListView {
        id: list

        anchors.fill: parent
        anchors.margins: 8
        spacing: 2
        clip: true

        // Stay pinned to the tail as lines arrive, but let the user scroll back without
        // being yanked forward again.
        property bool at_tail: true

        onCountChanged: if (at_tail) positionViewAtEnd()
        onMovementEnded: at_tail = atYEnd

        delegate: Text {
            required property string modelData

            width: list.width
            text: modelData
            color: Colour.log_text
            font.family: "Consolas, monospace"
            font.pixelSize: 12
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
    }

    Text {
        anchors.centerIn: parent
        visible: list.count === 0
        text: qsTr("Waiting for device logs…")
        color: Colour.log_text
        opacity: 0.6
        font: AppFont.hover_hint
    }
}
