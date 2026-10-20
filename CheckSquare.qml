import QtQuick

// Visual-only checkbox square. Parent handles input.
Rectangle {
    id: box
    property bool checked: false
    property bool partial: false
    property bool hovered: false

    implicitWidth: 16
    implicitHeight: 16
    radius: 4
    color: checked || partial ? Theme.accent : Theme.panel
    border.width: 1
    border.color: checked || partial ? Theme.accent : (hovered ? Theme.accent : "#9aa4b1")

    Canvas {
        anchors.fill: parent
        visible: box.checked && !box.partial
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = "white"
            ctx.lineWidth = 2
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(width * 0.26, height * 0.52)
            ctx.lineTo(width * 0.44, height * 0.70)
            ctx.lineTo(width * 0.76, height * 0.32)
            ctx.stroke()
        }
    }

    Rectangle {
        visible: box.partial
        anchors.centerIn: parent
        width: parent.width * 0.5
        height: 2
        radius: 1
        color: "white"
    }
}
