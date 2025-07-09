import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import App.Theme 0.1 as Theme

// import App.Model 0.1

Item {
    id: camera_list

    property int index: Theme.AppSettings.camera_detected ? 1 : 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            color: "transparent"
            border.color: "white"
            border.width: 1

            Layout.preferredWidth: 450
            Layout.preferredHeight: 110

            StackLayout {
                anchors.fill: parent
                currentIndex: camera_list.index

                Item {
                    Label {
                        text: qsTr("No Camera Found")
                        anchors.centerIn: parent
                        font: Theme.Font.camera_title
                    }
                }

                ListView {
                    id: connected_camera_list

                    model: CameraModel
                    boundsBehavior: Flickable.StopAtBounds
                    focus: true

                    delegate: ItemDelegate {
                        id: delegateRoot

                        required property string name
                        required property int index

                        text: name
                        // text: model.display
                        width: parent.width - scroll_bar.width
                        font: Theme.Font.camera_name

                        highlighted: ListView.isCurrentItem

                        background: Rectangle {
                            color: delegateRoot.highlighted ? Theme.Color.accent : "transparent"
                        }

                        onClicked: {
                            connected_camera_list.currentIndex = index;
                            AppSettings.selected_camera_index = index;
                            console.log("clicked:", name);
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: scroll_bar
                        policy: ScrollBar.AlwaysOn
                    }
                }
            }
        }

        Rectangle {
            id: camera_count
            color: "transparent"
            border.color: "white"
            border.width: 1
            Layout.alignment: Qt.AlignCenter

            Layout.preferredWidth: 90
            Layout.preferredHeight: 110

            StackLayout {
                anchors.fill: parent
                currentIndex: camera_list.index

                Image {
                    source: "qrc:/camera-connect.svg"
                    fillMode: Image.PreserveAspectFit
                }

                Image {
                    source: "qrc:/camera-connected.svg"
                    fillMode: Image.PreserveAspectFit

                    Label {
                        text: qsTr("%1").arg(connected_camera_list.count)
                        font: Theme.Font.camera_count
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -25
                    }
                }
            }
        }
    }
}
