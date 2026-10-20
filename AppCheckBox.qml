import QtQuick
import QtQuick.Controls.Basic

CheckBox {
    id: control
    spacing: 8
    padding: 0
    hoverEnabled: true
    font.pixelSize: 13

    indicator: CheckSquare {
        x: control.leftPadding
        y: (control.height - height) / 2
        checked: control.checked
        hovered: control.hovered
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
