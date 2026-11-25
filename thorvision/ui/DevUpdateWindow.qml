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
    readonly property string selectedChannel: channel.currentIndex < 0 ? "" : channel.currentValue
    onSelectedChannelChanged: sourceChanged(false)
    readonly property bool validSource: selectedChannel.length > 0 && Update.valid_device_address(deviceAddress.text)
    property string preferredVersion: initialVersion
    property bool initialized: false
    property bool loadAfterLauncherReview: false
    property bool checkComplete: false

    function loadVersions() {
        if (validSource)
            Update.load_package_versions(deviceAddress.text, selectedChannel, false)
    }
    function sourceChanged(debounce) {
        if (!initialized) return
        loadAfterLauncherReview = false
        checkComplete = false
        preferredVersion = ""
        lookupDelay.stop()
        Update.invalidate_package_versions()
        if (debounce)
            lookupDelay.restart()
        else
            loadVersions()
    }
    Component.onCompleted: {
        initialized = true
        if (developmentDevice.length && initialVersion.length) {
            loadAfterLauncherReview = true
            Update.review_package(deviceAddress.text, selectedChannel, initialVersion)
        } else if (developmentDevice.length) {
            loadVersions()
        }
    }
    onClosing: close => { if (Update.in_progress) close.accepted = false }

    Timer {
        id: lookupDelay
        interval: 400
        onTriggered: window.loadVersions()
    }
    Connections {
        target: Update
        function onState_changed() {
            window.checkComplete = Update.state === UpdateState.UpToDate
            if (window.loadAfterLauncherReview &&
                    (Update.state === UpdateState.Dismissed || Update.state === UpdateState.UpToDate)) {
                window.loadAfterLauncherReview = false
                window.loadVersions()
            }
        }
    }
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
            onTextChanged: window.sourceChanged(true)
            onAccepted: {
                lookupDelay.stop()
                window.loadVersions()
            }
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
        ComboBox {
            id: version
            Accessible.name: qsTr("Package version")
            Layout.fillWidth: true
            enabled: count > 0 && !Update.loading_package_versions
            model: {
                const versions = Update.package_versions
                const choices = []
                if (versions.length) {
                    choices.push(qsTr("Latest available"))
                    for (let i = 0; i < versions.length; ++i)
                        choices.push(versions[i])
                }
                return choices
            }
            currentIndex: !count ? -1 : window.preferredVersion.length ? model.indexOf(window.preferredVersion) : 0
            displayText: currentIndex < 0 ? qsTr("Choose a version") : currentText
            onActivated: index => { window.preferredVersion = index === 0 ? "" : model[index] }
        }
        RowLayout {
            Button {
                text: qsTr("Review package")
                enabled: version.enabled && version.currentIndex >= 0 && window.validSource
                onClicked: Update.review_package(deviceAddress.text, window.selectedChannel,
                                                 version.currentIndex === 0 ? "" : version.currentText)
            }
            Button {
                text: qsTr("Refresh versions")
                enabled: window.validSource && !Update.loading_package_versions
                onClicked: {
                    lookupDelay.stop()
                    window.loadVersions()
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Update.state === UpdateState.Checking ? qsTr("Checking the device and package catalog…")
                  : Update.loading_package_versions ? qsTr("Loading versions…")
                  : Update.package_list_message.length ? Update.package_list_message
                  : window.checkComplete ? qsTr("Package check complete.") : ""
            visible: text.length > 0
        }
        Button {
            text: qsTr("This is an AIO")
            visible: Update.package_list_needs_aio_confirmation
            onClicked: Update.load_package_versions(deviceAddress.text, window.selectedChannel, true)
        }
    }
    UpdateDialog {}
}
