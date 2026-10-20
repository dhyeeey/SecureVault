import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root

    property string localError: ""
    property bool showPassword: false
    readonly property bool creating: !vault.vaultExists
    readonly property string errorText: localError.length ? localError : vault.lastError
    readonly property bool mismatch: creating && confirmField.text.length > 0
                                     && confirmField.text !== passwordField.text

    function submit() {
        if (vault.busy)
            return
        localError = ""
        if (!creating) {
            if (passwordField.text.length === 0)
                return
            vault.unlock(vault.vaultPath, passwordField.text)
        } else {
            if (passwordField.text.length < 8) {
                localError = qsTr("Password must be at least 8 characters.")
                return
            }
            if (passwordField.text !== confirmField.text) {
                localError = qsTr("Passwords do not match.")
                return
            }
            vault.createVault(vault.vaultPath, passwordField.text)
        }
        passwordField.text = ""
        confirmField.text = ""
        showPassword = false
    }

    onVisibleChanged: {
        if (visible) {
            localError = ""
            passwordField.forceActiveFocus()
        }
    }

    Component.onCompleted: passwordField.forceActiveFocus()

    FolderDialog {
        id: vaultFolderDialog
        title: qsTr("Open a different vault")
        onAccepted: {
            appSettings.vaultPath = selectedFolder.toString()
            root.localError = ""
            passwordField.text = ""
            confirmField.text = ""
            passwordField.forceActiveFocus()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Theme.header }
            GradientStop { position: 1; color: Theme.headerHi }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 12

            Label {
                text: qsTr("SecureVault")
                color: "white"
                font.pixelSize: 16
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("XChaCha20-Poly1305 · Argon2id")
                color: Theme.headerMuted
                font.pixelSize: 11
            }
        }
    }

    Item {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Rectangle {
            id: card
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 420)
            height: form.implicitHeight + 56
            radius: 12
            color: Theme.panel
            border.color: Theme.line

            ColumnLayout {
                id: form
                anchors.fill: parent
                anchors.margins: 28
                spacing: 10

                VaultGlyph {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 76
                    Layout.preferredHeight: 76
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.creating ? qsTr("Create a new vault") : qsTr("Unlock your vault")
                    font.pixelSize: 22
                    font.bold: true
                    color: Theme.text
                }

                Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 6
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    font.pixelSize: 13
                    text: root.creating
                          ? qsTr("No vault found yet. Pick a password you won't forget; there is no way to recover it.")
                          : qsTr("Enter your password to decrypt your files.")
                }

                AppTextField {
                    id: passwordField
                    Layout.fillWidth: true
                    echoMode: root.showPassword ? TextInput.Normal : TextInput.Password
                    placeholderText: root.creating ? qsTr("New password (8+ characters)")
                                                   : qsTr("Password")
                    rightPadding: 60
                    enabled: !vault.busy
                    onTextEdited: root.localError = ""
                    Keys.onReturnPressed: root.submit()
                    Keys.onEnterPressed: root.submit()

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        visible: passwordField.text.length > 0
                        text: root.showPassword ? qsTr("Hide") : qsTr("Show")
                        font.pixelSize: 12
                        font.bold: true
                        color: toggleMouse.containsMouse ? Theme.accentHover : Theme.accent
                        MouseArea {
                            id: toggleMouse
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.showPassword = !root.showPassword
                        }
                    }
                }

                AppTextField {
                    id: confirmField
                    Layout.fillWidth: true
                    visible: root.creating
                    echoMode: root.showPassword ? TextInput.Normal : TextInput.Password
                    placeholderText: qsTr("Confirm password")
                    invalid: root.mismatch
                    enabled: !vault.busy
                    onTextEdited: root.localError = ""
                    Keys.onReturnPressed: root.submit()
                    Keys.onEnterPressed: root.submit()
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.errorText.length > 0
                    implicitHeight: errorLabel.implicitHeight + 16
                    radius: Theme.radius
                    color: Theme.dangerSoft
                    border.color: "#fecaca"

                    Label {
                        id: errorLabel
                        anchors.fill: parent
                        anchors.margins: 8
                        anchors.leftMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                        text: root.errorText
                        color: Theme.danger
                        font.pixelSize: 12
                    }
                }

                AppButton {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    implicitHeight: 40
                    variant: "primary"
                    font.bold: true
                    font.pixelSize: 14
                    enabled: !vault.busy && passwordField.text.length > 0
                             && (!root.creating || confirmField.text.length > 0)
                    text: vault.busy ? (root.creating ? qsTr("Creating vault…") : qsTr("Unlocking…"))
                                     : (root.creating ? qsTr("Create vault") : qsTr("Unlock"))
                    onClicked: root.submit()
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 2
                    text: qsTr("Open a different vault…")
                    font.pixelSize: 12
                    font.underline: openOtherMouse.containsMouse
                    color: !vault.busy && openOtherMouse.containsMouse ? Theme.accentHover : Theme.accent
                    opacity: vault.busy ? 0.5 : 1

                    MouseArea {
                        id: openOtherMouse
                        anchors.fill: parent
                        anchors.margins: -4
                        enabled: !vault.busy
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: vaultFolderDialog.open()
                    }
                }
            }
        }
    }
}
