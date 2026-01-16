pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Rectangle {
    color: Theme.Color.camera_settings

    Rectangle {
        color: Theme.Color.camera_settings_border
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
                mirror: Theme.AppSettings.camera_settings_visible
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                Theme.AppSettings.camera_settings_visible = !Theme.AppSettings.camera_settings_visible;
            }
        }
    }

    Drawer {
        id: settings_drawer
        modal: false
        edge: Qt.RightEdge
        interactive: false
        visible: Theme.AppSettings.camera_settings_visible
        enabled: !Theme.AppSettings.recording

        y: 111 + 19 + 2
        width: Theme.AppSettings.camera_settings_visible ? 265 : 0
        height: Screen.desktopAvailableHeight - y

        background: Rectangle {
            color: "transparent"
            anchors.fill: parent
        }

        Item {
            id: camera_settings

            property var camera: null
            property var caps: null
            property var codecs: null

            Connections {
                target: CameraModel
                function onSelected_camera_changed() {
                    camera_settings.camera = CameraModel.get(CameraModel.selected_camera_index).camera_item;
                    camera_settings.caps = camera_settings.camera ? camera_settings.camera.caps : [];
                    camera_settings.codecs = camera_settings.camera ? camera_settings.camera.codecs : [];
                    caps_box.currentIndex = camera_settings.caps.indexOf(camera_settings.camera ? camera_settings.camera.cap : "");
                    codecs_box.currentIndex = camera_settings.codecs.indexOf(camera_settings.camera ? camera_settings.camera.codec : "");
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
                        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

                        Item {
                            RowLayout {
                                anchors.fill: parent

                                Label {
                                    text: qsTr("No Camera Found")
                                    font: Theme.Font.camera_settings_name

                                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                    Layout.leftMargin: 12
                                }
                            }
                        }

                        CameraNameEditor {
                            camera: camera_settings.camera
                            index: CameraModel.selected_camera_index
                        }
                    }
                }

                ColumnLayout {
                    spacing: 13
                    enabled: Theme.AppSettings.camera_detected

                    RowLayout {
                        spacing: 10

                        Label {
                            text: qsTr("Quality")
                            font: Theme.Font.camera_settings_text
                            color: Theme.Color.text
                        }

                        CustomComboBox {
                            id: caps_box
                            model: camera_settings.caps

                            Layout.preferredWidth: 172

                            delegate: ItemDelegate {
                                id: cap_delegate
                                width: caps_box.width

                                required property int index

                                contentItem: Text {
                                    text: camera_settings.caps[cap_delegate.index]
                                    color: Theme.Color.text
                                }
                                background: Rectangle {
                                    color: cap_delegate.highlighted ? Theme.Color.accent : camera_settings.camera.cap_selectable(camera_settings.caps[cap_delegate.index]) ? "transparent" : Theme.Color.warn
                                }
                                highlighted: ListView.isCurrentItem

                                onClicked: {
                                    if (camera_settings.camera.cap_selectable(camera_settings.caps[index])) {
                                        caps_box.currentIndex = index;
                                        camera_settings.camera.set_cap(camera_settings.caps[index]);
                                    } else {
                                        caps_box.currentIndex = index;
                                        camera_settings.camera.set_cap(camera_settings.caps[index]);
                                        codecs_box.currentIndex = 0;
                                        camera_settings.camera.set_codec("");
                                    }
                                    caps_box.popup.close();
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 14

                        Label {
                            text: qsTr("Codec")
                            font: Theme.Font.camera_settings_text
                            color: Theme.Color.text
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
                                    text: camera_settings.codecs[codec_delegate.index]
                                    color: Theme.Color.text
                                }
                                background: Rectangle {
                                    color: codec_delegate.highlighted ? Theme.Color.accent : camera_settings.camera.codec_selectable(camera_settings.codecs[codec_delegate.index]) ? "transparent" : Theme.Color.warn
                                }
                                highlighted: ListView.isCurrentItem

                                onClicked: {
                                    if (camera_settings.camera.codec_selectable(camera_settings.codecs[index])) {
                                        codecs_box.currentIndex = index;
                                        camera_settings.camera.set_codec(camera_settings.codecs[index]);
                                    } else {
                                        caps_box.currentIndex = 0;
                                        camera_settings.camera.set_cap("");
                                        codecs_box.currentIndex = index;
                                        camera_settings.camera.set_codec(camera_settings.codecs[index]);
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
