pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 

Rectangle {
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

    Drawer {
        id: settings_drawer
        modal: false
        edge: Qt.RightEdge
        interactive: false
        visible: AppSettings.camera_settings_visible
        enabled: !Recorder.recording

        y: 111 + 19 + 2
        width: AppSettings.camera_settings_visible ? 265 : 0
        height: Screen.desktopAvailableHeight - y

        background: Rectangle {
            color: "transparent"
            anchors.fill: parent
        }

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
            }
        }
    }
}
