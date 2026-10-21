import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1

// "SSD REC" / "PC Off" badge on a video preview. Names the recorder so the two are never
// confused: SSD recording runs on the device, PC recording runs in this app.
Rectangle {
    id: chip

    property string recorder: ""
    property string state_text: ""
    property string kind: "off"

    implicitWidth: row.implicitWidth + 18
    implicitHeight: 24
    radius: 3
    color: Qt.rgba(0.08, 0.08, 0.08, 0.78)
    border.color: Qt.rgba(1, 1, 1, 0.12)
    border.width: 1

    RowLayout {
        id: row

        anchors.centerIn: parent
        spacing: 6

        StatusIcon {
            kind: chip.kind

            Layout.preferredWidth: 11
            Layout.preferredHeight: 11
        }

        Label {
            text: chip.recorder
            font.family: AppFont.helvetica
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
            color: Colour.text
        }

        Label {
            text: chip.state_text
            font: AppFont.status_small
            color: Colour.text
        }
    }
}
