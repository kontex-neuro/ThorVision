import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import App.Theme 0.1

ApplicationWindow {
    id: window
    visible: true
    width: 920
    height: 650
    minimumWidth: 880
    minimumHeight: 580
    title: deviceAddress.text.length ? qsTr("ThorVision AIO updater — %1").arg(deviceAddress.text) : qsTr("ThorVision AIO updater")
    function review() {
        Update.review_package(deviceAddress.text, channel.currentIndex < 0 ? "" : channel.currentValue, version.text)
    }
    Component.onCompleted: { if (developmentDevice.length > 0) review() }
    onClosing: close => { if (Update.in_progress) close.accepted = false }

    ColumnLayout {
        anchors.centerIn: parent
        width: 420
        spacing: 10
        enabled: !Update.in_progress && Update.state !== UpdateState.Checking
        Label { text: qsTr("Device IPv4 address") }
        TextField {
            id: deviceAddress
            Accessible.name: qsTr("Device IPv4 address")
            Layout.fillWidth: true
            text: developmentDevice
            placeholderText: qsTr("e.g. 192.168.177.100")
        }
        Label { text: qsTr("Channel") }
        ComboBox {
            id: channel
            Accessible.name: qsTr("Channel")
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "value"
            model: [{label: qsTr("Production"), value: "release"}, {label: qsTr("DEV"), value: "dev"}]
            currentIndex: initialChannel === "release" ? 0 : initialChannel === "dev" ? 1 : -1
        }
        Label { text: qsTr("Package version") }
        TextField {
            id: version
            Accessible.name: qsTr("Package version")
            Layout.fillWidth: true
            text: initialVersion
            placeholderText: qsTr("Latest available")
            onAccepted: window.review()
        }
        Button {
            text: qsTr("Review package")
            onClicked: window.review()
        }
        Label {
            text: Update.state === UpdateState.UpToDate ? qsTr("Package check complete.")
                  : Update.state === UpdateState.Checking ? qsTr("Checking the device and package catalog…") : ""
            visible: text.length > 0
        }
    }
    UpdateDialog {}
}
