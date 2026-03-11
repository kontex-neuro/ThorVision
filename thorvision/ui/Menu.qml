import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

import App.Theme 0.1

MenuBar {
    Menu {
        title: qsTr("&File")

        Connections {
            target: Config
            function onImport_failed(reason) {
                dialog_label.text = qsTr("Import failed: %1").arg(reason);
                import_failed_dialog.open();
            }
        }

        FileDialog {
            id: file_dialog

            defaultSuffix: "json"
            nameFilters: ["JSON files (*.json)"]
            currentFolder: Config.default_config_path()

            onAccepted: {
                if (fileMode === FileDialog.OpenFile) {
                    var success = Config.import_from_file(selectedFile);
                    if (success) {
                        Bus.status_notify("Import successful.");
                    } else {
                        import_failed_dialog.open();
                    }
                } else if (fileMode === FileDialog.SaveFile) {
                    let result = Config.export_to_file(selectedFile);
                    Bus.status_notify("Configuration saved.");
                }
            }
        }

        FileDialog {
            id: default_config_dialog

            fileMode: FileDialog.OpenFile
            nameFilters: ["JSON files (*.json)"]
            onAccepted: {
                let success = Config.set_default(selectedFile);
                if (success) {
                    Bus.status_notify("Default config set.");
                } else {
                    import_failed_dialog.open();
                }
            }
        }

        AlertDialog {
            id: import_failed_dialog
            title_text: qsTr("Import Failed")
            content_data: Label {
                id: dialog_label
                anchors.top: parent.top
                anchors.topMargin: 197
                anchors.horizontalCenter: parent.horizontalCenter

                width: parent.width - 200
                text: qsTr("Import failed: unsupported file format.")
                font: AppFont.popup_text
                color: Color.text
                lineHeightMode: Text.FixedHeight
                lineHeight: 30

                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            footer_data: CustomDialogButton {
                button_text: qsTr("OK")
                anchors.bottom: parent.bottom
                anchors.right: parent.right

                onClicked: import_failed_dialog.close()
            }
        }

        Action {
            text: qsTr("&Load Config")
            shortcut: StandardKey.Open
            onTriggered: {
                file_dialog.fileMode = FileDialog.OpenFile;
                file_dialog.open();
            }
        }
        Action {
            text: qsTr("&Save Config")
            shortcut: StandardKey.Save
            onTriggered: {
                file_dialog.fileMode = FileDialog.SaveFile;
                file_dialog.open();
            }
        }
        MenuSeparator {}
        Action {
            text: qsTr("&Set Default Config")
            shortcut: "Ctrl+D"
            onTriggered: {
                default_config_dialog.open();
            }
        }
    }
    Menu {
        title: qsTr("&Help")

        AlertDialog {
            id: license_dialog
            title_text: qsTr("License")
            icon_source: "qrc:/qt/qml/App/Theme/resources/license.svg"
            content_data: Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 12

                color: Color.popup_header
                width: 704
                height: 360
                radius: 1

                ScrollView {
                    anchors.fill: parent
                    // hack: manually set margin to center text with button
                    anchors.leftMargin: 36
                    clip: true

                    ScrollBar.vertical.visible: true
                    ScrollBar.horizontal.visible: false

                    Label {
                        anchors.fill: parent

                        text: Config.license_text()
                        wrapMode: Text.Wrap
                        font: AppFont.popup_scroll_text
                        color: Color.text

                        lineHeightMode: Text.FixedHeight
                        lineHeight: 23

                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
            footer_data: CustomDialogButton {
                button_text: qsTr("OK")

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -15

                onClicked: license_dialog.close()
            }
        }

        Action {
            text: qsTr("&Online Documentation")
            onTriggered: {
                Qt.openUrlExternally(AppSettings.doc);
            }
        }
        Action {
            text: qsTr("&View License")
            onTriggered: {
                license_dialog.open();
            }
        }
        Action {
            text: qsTr("&Report Issue")
            onTriggered: {
                Qt.openUrlExternally(AppSettings.report_issue);
            }
        }
    }
}
