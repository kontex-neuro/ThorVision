pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

Item {
    id: setting_view

    Drawer {
        id: settings_drawer
        modal: false
        edge: Qt.RightEdge
        interactive: false
        visible: Theme.AppSettings.camera_settings_visible

        y: 111 + 19 + 2
        height: Screen.desktopAvailableHeight - y
        width: 265

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
                    console.log("onSelected_camera_changed", camera_settings.camera, camera_settings.caps, camera_settings.codecs, caps_box.currentIndex, codecs_box.currentIndex);
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
                        fillMode: Image.PreserveAspectFit

                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 15
                    }

                    StackLayout {
                        currentIndex: Theme.AppSettings.camera_detected ? 1 : 0

                        Item {
                            Label {
                                text: qsTr("No Camera Found")
                                font: Theme.Font.camera_settings_name
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                            }
                        }

                        CameraNameEditor {
                            camera: camera_settings.camera
                        }
                    }
                }

                Item {
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
                                        console.log("onClicked", index, camera_settings.caps[index]);
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
                            spacing: 10

                            Label {
                                text: qsTr("Format")
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
                                        console.log("onClicked", index, camera_settings.codecs[index]);
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
}
