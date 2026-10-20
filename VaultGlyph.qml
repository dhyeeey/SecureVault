import QtQuick

Item {
    id: root
    implicitWidth: 160
    implicitHeight: 160

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.82
        height: width * 0.92
        radius: 18
        color: "#d7dde3"
        border.color: "#9aa3ad"
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            radius: 12
            color: "#eef1f4"
            border.color: "#b7c0c8"
            border.width: 1

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.46
                height: width
                radius: width / 2
                color: "#8f9aa6"
                border.color: "#5e6872"
                border.width: 3

                Repeater {
                    model: 8
                    Rectangle {
                        required property int index
                        width: 6
                        height: 12
                        radius: 2
                        color: "#e9edf1"
                        x: parent.width / 2 - width / 2
                        y: 8
                        transformOrigin: Item.Bottom
                        rotation: index * 45
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.verticalCenter
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.28
                    height: width
                    radius: width / 2
                    color: "#dfe4e9"
                    border.color: "#6b747e"
                    border.width: 2
                }
            }
        }

        Rectangle {
            width: 10
            height: parent.height * 0.28
            radius: 4
            color: "#7b8793"
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 4
        }
    }
}
