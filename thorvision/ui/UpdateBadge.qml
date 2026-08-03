import QtQuick
import QtQuick.Controls

import App.Theme 0.1

// Small dot on the XDAQ status block. Visible only when an update was offered and set
// aside, or when one failed; clicking it brings the dialog back.
Rectangle {
    id: root

    readonly property bool pending: Update.update_dismissed && (Update.state === UpdateState.Dismissed || Update.state === UpdateState.Failed)

    // Above the catch-all MouseArea in XDAQStatus, which would otherwise swallow the click.
    z: 10

    width: 12
    height: 12
    radius: 6
    visible: pending
    color: Update.required ? Colour.update_badge_required : Colour.update_badge
    border.color: Colour.border_2
    border.width: 1

    SequentialAnimation on opacity {
        running: root.visible && Update.required
        loops: Animation.Infinite

        NumberAnimation {
            from: 1.0
            to: 0.45
            duration: 900
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            from: 0.45
            to: 1.0
            duration: 900
            easing.type: Easing.InOutQuad
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        anchors.margins: -6  // easier to hit than 12px
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: Update.check_for_update()
    }

    ToolTip {
        visible: mouse.containsMouse
        text: Update.required ? qsTr("A required server update is pending. Click to review.") : qsTr("A server update is available. Click to review.")
    }
}
