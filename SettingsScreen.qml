import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root
    signal back()

    property bool passwordPending: false

    function syncPrefs() {
        lockCombo.currentIndex = Math.max(0, lockCombo.indexOfValue(vault.autoLockMinutes))
    }

    onVisibleChanged: {
        if (visible)
            syncPrefs()
    }

    Connections {
        target: vault
        function onVaultPrefsChanged() { root.syncPrefs() }
        function onLastErrorChanged() {
            if (root.visible && !root.passwordPending && vault.lastError.length > 0)
                toast.show(vault.lastError, "error")
        }
    }

    Connections {
        target: vault
        function onBusyChanged() {
            if (vault.busy || !root.passwordPending)
                return
            root.passwordPending = false
            if (vault.lastError.length > 0)
                toast.show(vault.lastError, "error")
            else
                toast.show(qsTr("Password changed."))
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !passwordDialog.visible
        onActivated: root.back()
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Switch to another vault")
        onAccepted: {
            const target = selectedFolder.toString()
            vault.lock()
            appSettings.vaultPath = target
        }
    }

    ChangePasswordDialog {
        id: passwordDialog
        onAccepted: root.passwordPending = true
    }

    // ---------- inline components ----------

    component SettingsCard: Rectangle {
        id: card
        property string title
        default property alias content: body.data
        Layout.fillWidth: true
        implicitHeight: head.implicitHeight + body.implicitHeight + 16 + 14 + 20
        radius: 10
        color: Theme.panel
        border.color: Theme.line

        Label {
            id: head
            x: 20
            y: 16
            text: card.title
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 0.6
            font.capitalization: Font.AllUppercase
            color: Theme.muted
        }

        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: head.bottom
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: 14
            spacing: 16
        }
    }

    component SettingRow: RowLayout {
        id: settingRow
        property string title
        property string description
        default property alias control: slot.data
        Layout.fillWidth: true
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: settingRow.title
                font.pixelSize: 14
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: settingRow.description
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Theme.muted
            }
        }
        Item {
            id: slot
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }

    component AppSwitch: Switch {
        id: sw
        padding: 0
        implicitWidth: 40
        implicitHeight: 22
        hoverEnabled: true
        indicator: Rectangle {
            width: 40
            height: 22
            radius: 11
            color: sw.checked ? (sw.hovered ? Theme.accentHover : Theme.accent)
                              : (sw.hovered ? "#b8c1cc" : "#cbd2db")
            Rectangle {
                x: sw.checked ? parent.width - width - 3 : 3
                y: 3
                width: 16
                height: 16
                radius: 8
                color: "white"
                Behavior on x { NumberAnimation { duration: 120 } }
            }
        }
        contentItem: Item {}
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.line
    }

    // ---------- layout ----------

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
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            AppButton {
                text: qsTr("‹  Back")
                variant: "header"
                focusPolicy: Qt.NoFocus
                onClicked: root.back()
            }
            Label {
                text: qsTr("Settings")
                color: "white"
                font.pixelSize: 18
                font.bold: true
                Layout.fillWidth: true
            }
        }
    }

    Flickable {
        id: flick
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentHeight: content.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: content
            width: Math.min(flick.width - 48, 680)
            x: (flick.width - width) / 2
            y: 24
            spacing: 16

            SettingsCard {
                title: qsTr("Vault")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: qsTr("Vault location")
                        font.pixelSize: 14
                        color: Theme.text
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: Theme.muted
                        text: qsTr("The folder that holds this vault's encrypted files. Switching to another vault locks this one first.")
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        implicitHeight: 36
                        radius: Theme.radius
                        color: Theme.subtle
                        border.color: Theme.line

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                            font.pixelSize: 13
                            color: Theme.text
                            text: vault.vaultPath
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Item { Layout.fillWidth: true }
                        AppButton {
                            text: qsTr("Switch vault…")
                            variant: "secondary"
                            enabled: !vault.busy
                            onClicked: folderDialog.open()
                        }
                        AppButton {
                            text: qsTr("Lock now")
                            variant: "secondary"
                            enabled: !vault.busy
                            onClicked: vault.lock()
                        }
                    }
                }
            }

            SettingsCard {
                title: qsTr("Security")

                SettingRow {
                    title: qsTr("Auto-lock")
                    description: qsTr("Lock the vault and wipe keys from memory after this much inactivity.")

                    ComboBox {
                        id: lockCombo
                        implicitWidth: 150
                        implicitHeight: 34
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: qsTr("Never"), value: 0 },
                            { text: qsTr("1 minute"), value: 1 },
                            { text: qsTr("5 minutes"), value: 5 },
                            { text: qsTr("15 minutes"), value: 15 },
                            { text: qsTr("30 minutes"), value: 30 },
                            { text: qsTr("1 hour"), value: 60 }
                        ]
                        enabled: !vault.busy
                        Component.onCompleted: root.syncPrefs()
                        onActivated: vault.autoLockMinutes = currentValue

                        background: Rectangle {
                            radius: Theme.radius
                            color: lockCombo.hovered ? "#f1f5f9" : Theme.panel
                            border.color: lockCombo.activeFocus ? Theme.accent : Theme.line
                        }
                    }
                }

                Divider {}

                SettingRow {
                    title: qsTr("Password")
                    description: qsTr("Re-wrap the vault key with a new password.")

                    AppButton {
                        text: qsTr("Change password…")
                        variant: "secondary"
                        enabled: vault.unlocked && !vault.busy
                        onClicked: passwordDialog.open()
                    }
                }
            }

            SettingsCard {
                title: qsTr("Behavior")

                SettingRow {
                    title: qsTr("Confirm before deleting")
                    description: qsTr("Ask \"Are you sure?\" before removing files or folders from the vault.")

                    AppSwitch {
                        checked: vault.confirmDelete
                        enabled: !vault.busy
                        onToggled: vault.confirmDelete = checked
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 4
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Theme.muted
                text: qsTr("Auto-lock and behavior settings are stored encrypted inside this vault, so they can only be changed after unlocking it.")
            }

            SettingsCard {
                title: qsTr("About")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: qsTr("SecureVault %1").arg(Qt.application.version)
                        font.pixelSize: 14
                        color: Theme.text
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: Theme.muted
                        text: qsTr("Files are encrypted with XChaCha20-Poly1305. Your password is stretched with Argon2id. There is no password recovery.")
                    }
                }
            }
        }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
    }
}
