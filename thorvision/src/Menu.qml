import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

import App.Theme 0.1 as Theme

MenuBar {
    Menu {
        title: qsTr("&File")

        FileDialog {
            id: file_dialog

            defaultSuffix: "json"
            nameFilters: ["JSON files (*.json)"]
            currentFolder: StandardPaths.standardLocations(StandardPaths.AppDataLocation)[0]
            onAccepted: {
                if (fileMode === FileDialog.OpenFile) {
                    var success = Profiles.import_from_file(selectedFile);
                    if (success) {
                        Bus.status_notify("Import successful.");
                    } else {
                        import_failed_dialog.open();
                    }
                } else if (fileMode === FileDialog.SaveFile) {
                    let result = Profiles.export_to_file(selectedFile);
                    Bus.status_notify("Configuration saved.");
                }
            }
        }

        AlertDialog {
            id: import_failed_dialog
            title_text: qsTr("Import Failed")
            content_data: Label {
                anchors.top: parent.top
                anchors.topMargin: 197
                anchors.horizontalCenter: parent.horizontalCenter

                text: qsTr("Import failed: unsupported configuration file.")
                font: Theme.Font.popup_text
                color: Theme.Color.text
                lineHeightMode: Text.FixedHeight
                lineHeight: 30
                horizontalAlignment: Text.AlignHCenter
            }
            footer_data: CustomDialogButton {
                button_text: qsTr("OK")
                anchors.bottom: parent.bottom
                anchors.right: parent.right

                onClicked: {
                    import_failed_dialog.close();
                }
            }
        }

        Action {
            text: qsTr("&Load Settings")
            shortcut: StandardKey.Open
            onTriggered: {
                file_dialog.fileMode = FileDialog.OpenFile;
                file_dialog.open();
            }
        }
        Action {
            text: qsTr("&Save Settings")
            shortcut: StandardKey.Save
            onTriggered: {
                file_dialog.fileMode = FileDialog.SaveFile;
                file_dialog.open();
            }
        }
    }
    Menu {
        title: qsTr("&Help")
        Action {
            text: qsTr("&Online Documentation")
            onTriggered: {
                Qt.openUrlExternally(Theme.AppSettings.doc);
            }
        }
        Action {
            text: qsTr("&View License")
            onTriggered: {
                Qt.openUrlExternally(Theme.AppSettings.license);
            }
        }
        Action {
            text: qsTr("&Report Issue")
            onTriggered: {
                Qt.openUrlExternally(Theme.AppSettings.report_issue);
            }
        }
    }
}
