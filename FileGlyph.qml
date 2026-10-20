import QtQuick

Item {
    id: root
    property bool isDir: false
    implicitWidth: 16
    implicitHeight: 16
    width: 16
    height: 16

    Rectangle {
        visible: root.isDir
        width: 15
        height: 11
        radius: 2
        y: 4
        color: "#e2b340"
        border.color: "#c4921d"
        border.width: 1
    }
    Rectangle {
        visible: root.isDir
        width: 7
        height: 4
        radius: 1
        x: 1
        y: 2
        color: "#f0c75a"
        border.color: "#c4921d"
        border.width: 1
    }

    Rectangle {
        visible: !root.isDir
        width: 12
        height: 15
        radius: 1
        x: 2
        color: "#f8fafc"
        border.color: "#7b8794"
        border.width: 1
    }
    Rectangle {
        visible: !root.isDir
        width: 5
        height: 5
        x: 9
        color: "#e5e7eb"
        border.color: "#7b8794"
        border.width: 1
    }
}
