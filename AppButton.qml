import QtQuick
import QtQuick.Controls.Basic

// variant: "primary" | "secondary" | "ghost" | "danger" | "header"
Button {
    id: control
    property string variant: "secondary"
    hoverEnabled: true
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 13

    readonly property bool light: variant === "secondary" || variant === "ghost"

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: 32
        radius: Theme.radius
        opacity: control.enabled ? 1 : (control.light ? 1 : 0.45)
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: control.activeFocus ? Theme.accent : Theme.line
        color: {
            const down = control.down
            const hover = control.hovered && control.enabled
            switch (control.variant) {
            case "primary":
                return down ? "#1e40af" : (hover ? Theme.accentHover : Theme.accent)
            case "danger":
                return down ? "#7f1d1d" : (hover ? "#991b1b" : Theme.danger)
            case "header":
                return down ? "#222a30" : (hover ? Theme.headerButtonHover : Theme.headerButton)
            case "ghost":
                return down ? "#e2e8f0" : (hover ? Theme.rowHover : "transparent")
            default:
                return down ? "#e2e8f0" : (hover ? "#f1f5f9" : Theme.panel)
            }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.light ? Theme.text : "white"
        opacity: control.enabled ? 1 : (control.light ? 0.38 : 0.85)
    }
}
