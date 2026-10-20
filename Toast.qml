import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root
    property string message: ""
    property string kind: "info" // "info" | "error"

    function show(text, k) {
        message = text
        kind = k || "info"
        hideTimer.interval = kind === "error" ? 6000 : 2500
        hideTimer.restart()
        opacity = 1
    }

    visible: opacity > 0
    opacity: 0
    z: 100
    radius: 8
    height: 40
    width: Math.min(label.implicitWidth + 48, (parent ? parent.width : 600) - 40)
    color: kind === "error" ? "#7f1d1d" : "#1f2933"

    Behavior on opacity { NumberAnimation { duration: 160 } }

    Rectangle {
        width: 4
        height: parent.height - 16
        radius: 2
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        color: root.kind === "error" ? "#fca5a5" : "#93c5fd"
    }

    Label {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.right: parent.right
        anchors.rightMargin: 16
        text: root.message
        color: "white"
        elide: Text.ElideRight
        font.pixelSize: 13
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.opacity = 0
    }

    Timer {
        id: hideTimer
        onTriggered: root.opacity = 0
    }
}
