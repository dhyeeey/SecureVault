import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control
    property bool invalid: false
    implicitHeight: 36
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 13
    color: Theme.text
    placeholderTextColor: "#9aa4b1"
    selectByMouse: true
    selectionColor: "#bfdbfe"
    selectedTextColor: Theme.text

    background: Rectangle {
        implicitWidth: 200
        radius: Theme.radius
        color: control.enabled ? Theme.panel : Theme.subtle
        border.width: control.activeFocus ? 2 : 1
        border.color: control.invalid ? Theme.danger
                    : (control.activeFocus ? Theme.accent : Theme.line)
    }
}
