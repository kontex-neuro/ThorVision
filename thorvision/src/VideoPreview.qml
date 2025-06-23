import QtQuick 2.15

Item {
    id: root
    property int i: 0

    Image {
        id: video
        anchors.fill: parent
        source: "image://video/live"
        fillMode: Image.PreserveAspectFit
        cache: false
    }

    Timer {
        interval: 30
        running: true
        repeat: true
        onTriggered: video.source = "image://video/live?" + Date.now()
    }
}
