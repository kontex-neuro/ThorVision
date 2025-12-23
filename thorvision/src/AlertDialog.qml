import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Dialog {
    id: dialog

    modal: true
    parent: Overlay.overlay
    padding: 0
    spacing: 0
    width: 847
    height: 534
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)

    background: Rectangle {
        color: "transparent"
    }

    property alias title_text: title.text
    default property alias content_data: content.data
    property alias footer_data: footer.data

    contentItem: Rectangle {
        id: item
        color: Theme.Color.camera_settings
        radius: 15
        border.color: Theme.Color.popup_border
        border.width: 1

        ColumnLayout {
            anchors.fill: parent

            Rectangle {
                color: Theme.Color.popup_header
                topLeftRadius: item.radius
                topRightRadius: item.radius
                bottomLeftRadius: 0
                bottomRightRadius: 0

                Layout.fillWidth: true
                Layout.preferredHeight: 45
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: item.border.width
                Layout.leftMargin: item.border.width
                Layout.rightMargin: item.border.width

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    Image {
                        source: "qrc:/qt/qml/App/Theme/resources/warning.svg"
                        fillMode: Image.PreserveAspectFit

                        Layout.preferredWidth: 19
                        Layout.preferredHeight: 17
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        Layout.leftMargin: 35

                        Label {
                            id: title
                            color: Theme.Color.text
                            font: Theme.Font.popup_text

                            anchors.left: parent.left
                            anchors.leftMargin: 32
                        }
                    }
                }
            }

            Item {
                id: content

                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Item {
                id: footer

                Layout.fillWidth: true
                Layout.preferredHeight: 28
                Layout.leftMargin: 69
                Layout.rightMargin: 68
                Layout.bottomMargin: 47
            }
        }
    }
}
