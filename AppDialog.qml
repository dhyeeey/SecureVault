import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    margins: 0
    padding: 20
    topPadding: 12
    implicitWidth: 400
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton
    focus: true

    property string okText: qsTr("OK")
    property string cancelText: qsTr("Cancel")
    property bool okEnabled: true
    property bool showCancel: true
    property bool destructive: false

    Overlay.modal: Rectangle {
        color: "#59000000"
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 120; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
    }

    background: Rectangle {
        color: Theme.panel
        radius: 10
        border.color: Theme.line
        border.width: 1
    }

    header: Label {
        text: root.title
        visible: root.title.length > 0
        font.pixelSize: 16
        font.bold: true
        color: Theme.text
        leftPadding: 20
        rightPadding: 20
        topPadding: 18
    }

    footer: Item {
        implicitHeight: 60
        RowLayout {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 20
            spacing: 8

            AppButton {
                visible: root.showCancel
                text: root.cancelText
                variant: "secondary"
                implicitWidth: Math.max(88, implicitContentWidth + leftPadding + rightPadding)
                onClicked: root.reject()
            }

            AppButton {
                text: root.okText
                variant: root.destructive ? "danger" : "primary"
                enabled: root.okEnabled
                font.bold: true
                implicitWidth: Math.max(88, implicitContentWidth + leftPadding + rightPadding)
                onClicked: root.accept()
            }
        }
    }
}
