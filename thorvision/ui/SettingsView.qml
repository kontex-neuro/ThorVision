pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
    id: settings_root
    color: Colour.camera_settings

    Rectangle {
        color: Colour.camera_settings_border
        width: 21
        height: 21
        y: height
        x: -width - 2

        Item {
            width: 12
            height: 12
            anchors.centerIn: parent

            Image {
                source: "qrc:/qt/qml/App/Theme/resources/drawer.svg"
                fillMode: Image.PreserveAspectFit
                anchors.fill: parent
                mipmap: true
                mirror: AppSettings.camera_settings_visible
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                AppSettings.camera_settings_visible = !AppSettings.camera_settings_visible;
            }
        }
    }

    Item {
        id: settings_drawer
        anchors.fill: parent
        visible: AppSettings.camera_settings_visible
        enabled: DeviceRecording.connected && !DeviceRecording.busy

        Item {
            id: camera_settings

            property var camera: null
            property string name: camera ? camera.name : ""
            property var caps: camera ? camera.caps : null
            property var codecs: camera ? camera.codecs : null

            Connections {
                target: CameraModel
                function onSelected_camera_changed(index) {
                    camera_settings.camera = CameraModel.get(index).camera_item;
                    if (!camera_settings.camera)
                        return;

                    camera_settings.name = camera_settings.camera.name;
                    camera_settings.caps = camera_settings.camera.caps;
                    camera_settings.codecs = camera_settings.camera.codecs;
                    caps_box.currentIndex = camera_settings.caps.indexOf(camera_settings.camera.cap);
                    codecs_box.currentIndex = camera_settings.codecs.indexOf(camera_settings.camera.codec);
                }
            }
            Connections {
                target: camera_settings.camera
                function onCodec_changed() {
                    const codec = camera_settings.camera.codec;
                    const index = camera_settings.codecs.indexOf(codec);
                    if (index !== -1) {
                        codecs_box.currentIndex = index;
                    }
                }
            }
            Connections {
                target: camera_settings.camera
                function onCap_changed() {
                    const cap = camera_settings.camera.cap;
                    const index = camera_settings.caps.indexOf(cap);
                    if (index !== -1) {
                        caps_box.currentIndex = index;
                    }
                }
            }

            ColumnLayout {
                anchors.top: parent.top
                anchors.topMargin: 26
                anchors.left: parent.left
                anchors.leftMargin: 16
                spacing: 36

                RowLayout {
                    spacing: 0

                    Image {
                        source: "qrc:/qt/qml/App/Theme/resources/camera-icon.svg"
                        sourceSize.width: 20
                        sourceSize.height: 15
                        fillMode: Image.PreserveAspectFit

                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    }

                    StackLayout {
                        currentIndex: AppSettings.camera_detected ? 1 : 0

                        Item {
                            RowLayout {
                                anchors.fill: parent

                                Label {
                                    text: qsTr("No Camera Found")
                                    font: AppFont.camera_settings_name

                                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                    Layout.leftMargin: 12
                                }
                            }
                        }

                        CameraNameEditor {
                            index: CameraModel.selected_camera_index
                        }
                    }
                }

                ColumnLayout {
                    spacing: 13
                    enabled: AppSettings.camera_detected

                    RowLayout {
                        spacing: 10

                        Label {
                            text: qsTr("Quality")
                            font: AppFont.camera_settings_text
                            color: Colour.text
                        }

                        StackLayout {
                            currentIndex: AppSettings.camera_detected ? 1 : 0

                            CustomComboBox {
                                Layout.preferredWidth: 172
                            }

                            CustomComboBox {
                                id: caps_box
                                model: camera_settings.camera.caps

                                Layout.preferredWidth: 172

                                delegate: ItemDelegate {
                                    id: cap_delegate
                                    width: caps_box.width

                                    required property int index

                                    contentItem: Text {
                                        text: camera_settings.camera.cap_display(camera_settings.caps[cap_delegate.index])
                                        color: Colour.text
                                    }
                                    background: Rectangle {
                                        color: cap_delegate.highlighted ? Colour.accent : camera_settings.camera.cap_selectable(camera_settings.caps[cap_delegate.index]) ? "transparent" : Colour.warn
                                    }
                                    highlighted: ListView.isCurrentItem

                                    onClicked: {
                                        if (camera_settings.camera.cap_selectable(camera_settings.caps[index])) {
                                            caps_box.currentIndex = index;
                                            camera_settings.camera.cap = camera_settings.caps[index];
                                        } else {
                                            caps_box.currentIndex = index;
                                            codecs_box.currentIndex = 0;
                                            camera_settings.camera.cap = camera_settings.caps[index];
                                            camera_settings.camera.codec = "";
                                        }
                                        caps_box.popup.close();
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 14

                        Label {
                            text: qsTr("Codec")
                            font: AppFont.camera_settings_text
                            color: Colour.text
                        }

                        StackLayout {
                            currentIndex: AppSettings.camera_detected ? 1 : 0

                            CustomComboBox {
                                Layout.preferredWidth: 172
                            }

                            CustomComboBox {
                                id: codecs_box
                                model: camera_settings.codecs

                                Layout.preferredWidth: 172

                                delegate: ItemDelegate {
                                    id: codec_delegate
                                    width: codecs_box.width

                                    required property int index

                                    contentItem: Text {
                                        text: camera_settings.camera.codec_display(camera_settings.codecs[codec_delegate.index])
                                        color: Colour.text
                                    }
                                    background: Rectangle {
                                        color: codec_delegate.highlighted ? Colour.accent : camera_settings.camera.codec_selectable(camera_settings.codecs[codec_delegate.index]) ? "transparent" : Colour.warn
                                    }
                                    highlighted: ListView.isCurrentItem

                                    onClicked: {
                                        if (camera_settings.camera.codec_selectable(camera_settings.codecs[index])) {
                                            codecs_box.currentIndex = index;
                                            camera_settings.camera.codec = camera_settings.codecs[index];
                                        } else {
                                            caps_box.currentIndex = 0;
                                            codecs_box.currentIndex = index;
                                            camera_settings.camera.cap = "";
                                            camera_settings.camera.codec = camera_settings.codecs[index];
                                        }
                                        codecs_box.popup.close();
                                    }
                                }
                            }
                        }
                    }
                }

                // Changes stay pending until applied: the source is shared by SSD and PC
                // recording, so applying restarts this camera's stream.
                ColumnLayout {
                    id: apply_section

                    readonly property bool pending: !!camera_settings.camera && camera_settings.camera.pending_source
                    readonly property var device_camera: camera_settings.camera ? SsdStatus.camera_for(camera_settings.camera.stable_id) : null

                    spacing: 10
                    visible: AppSettings.camera_detected

                    Layout.preferredWidth: 233
                    Layout.topMargin: -12

                    RowLayout {
                        visible: apply_section.pending
                        spacing: 8

                        Label {
                            text: qsTr("Changed")
                            font: AppFont.status_label
                            color: Colour.status_caution
                            leftPadding: 5
                            rightPadding: 5

                            background: Rectangle {
                                color: "transparent"
                                border.color: Colour.caution_panel_border
                                border.width: 1
                                radius: 2
                            }
                        }

                        Label {
                            text: qsTr("Applied: %1").arg(SsdStatus.profile_text(apply_section.device_camera ? apply_section.device_camera.profile : null))
                            font: AppFont.status_small
                            color: Colour.text_muted
                            elide: Text.ElideRight

                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        color: Colour.background_1
                        border.color: Colour.border_2
                        border.width: 1
                        radius: 3

                        Layout.fillWidth: true
                        Layout.preferredHeight: shared_note.implicitHeight + 18

                        Label {
                            id: shared_note

                            anchors.fill: parent
                            anchors.margins: 9
                            text: qsTr("Shared by <b>SSD</b> and <b>PC</b> recording. Applying restarts this camera's stream. Other cameras keep running.")
                            textFormat: Text.StyledText
                            font: AppFont.status_small
                            color: Colour.text
                            wrapMode: Text.WordWrap
                            lineHeight: 1.15
                        }
                    }

                    CompactButton {
                        text: qsTr("Apply camera settings…")
                        variant: apply_section.pending ? "primary" : "default"
                        implicitHeight: 28
                        enabled: apply_section.pending && DeviceRecording.connected && !DeviceRecording.busy

                        Layout.fillWidth: true

                        onClicked: source_confirmation.open()
                    }
                }
            }
        }
    }
    AlertDialog {
        id: source_confirmation

        readonly property var device_camera: camera_settings.camera ? SsdStatus.camera_for(camera_settings.camera.stable_id) : null

        title_text: qsTr("Apply camera settings to %1?").arg(camera_settings.name)
        footer_left_margin: 48
        footer_right_margin: 48
        footer_bottom_margin: 30

        onAccepted: DeviceRecording.apply_selected_source(true)

        content_data: ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 48
            anchors.rightMargin: 48
            anchors.topMargin: 26
            spacing: 16

            Rectangle {
                color: Colour.background_1
                border.color: Colour.border_2
                border.width: 1
                radius: 3

                Layout.fillWidth: true
                Layout.preferredHeight: 38

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 16

                    Label {
                        text: SsdStatus.profile_text(source_confirmation.device_camera ? source_confirmation.device_camera.profile : null)
                        font: AppFont.status_title
                        color: Colour.text_muted
                    }

                    Label {
                        text: "→"
                        font: AppFont.status_title
                        color: Colour.text
                    }

                    Label {
                        text: camera_settings.camera ? qsTr("%1  %2").arg(camera_settings.camera.codec).arg(camera_settings.camera.cap) : ""
                        font: AppFont.status_title
                        color: Colour.text
                        elide: Text.ElideRight

                        Layout.fillWidth: true
                    }
                }
            }

            Repeater {
                model: [
                    { kind: "caution", text: qsTr("%1's stream restarts. Its <b>preview</b>, <b>SSD recording</b> and <b>PC recording</b> are all affected.").arg(camera_settings.name) },
                    { kind: "off", text: qsTr("Current recording files are finalized. Recording resumes in new files once the camera is streaming.") },
                    { kind: "caution", text: qsTr("Expect a short gap in this camera's recording, usually a few seconds.") },
                    { kind: "ok", text: qsTr("The other cameras keep recording without interruption.") }
                ]

                delegate: RowLayout {
                    id: effect

                    required property var modelData

                    spacing: 12
                    Layout.fillWidth: true

                    StatusIcon {
                        kind: effect.modelData.kind
                        Layout.preferredWidth: 13
                        Layout.preferredHeight: 13
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 3
                    }

                    Label {
                        text: effect.modelData.text
                        textFormat: Text.StyledText
                        font: AppFont.status_title
                        color: Colour.text
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        footer_buttons: [
            CompactButton {
                text: qsTr("Cancel")
                variant: "ghost"
                implicitHeight: 28

                onClicked: source_confirmation.reject()
            },
            CompactButton {
                text: qsTr("Restart camera and apply")
                variant: "primary"
                implicitHeight: 28
                enabled: DeviceRecording.connected && !DeviceRecording.busy

                onClicked: source_confirmation.accept()
            }
        ]
    }
}
