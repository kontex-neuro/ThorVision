import QtQuick
import QtQuick.Controls
import App.Theme 0.1

ApplicationWindow {
    visible: true
    width: 920
    height: 650
    minimumWidth: 880
    minimumHeight: 580
    title: qsTr("ThorVision DEV updater — %1").arg(developmentDevice)
    Component.onCompleted: Update.check_development_update()
    onClosing: close => { if (Update.in_progress) close.accepted = false }

    Column {
        anchors.centerIn: parent
        spacing: 16
        Label { text: qsTr("Device: %1").arg(developmentDevice) }
        Label {
            text: Update.state === UpdateState.UpToDate ? qsTr("The device is up to date.")
                  : Update.state === UpdateState.Checking ? qsTr("Checking for a DEV update…") : ""
            visible: text.length > 0
        }
        Button {
            text: qsTr("Check for DEV update")
            enabled: !Update.in_progress && Update.state !== UpdateState.Checking
            onClicked: Update.check_development_update()
        }
    }
    UpdateDialog {}
}
