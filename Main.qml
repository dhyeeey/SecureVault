import QtQuick
import QtQuick.Controls.Basic

ApplicationWindow {
    id: window
    width: 1080
    height: 700
    minimumWidth: 820
    minimumHeight: 560
    visible: true
    title: settingsOpen && vault.unlocked ? qsTr("SecureVault — Settings")
                        : (vault.unlocked ? qsTr("SecureVault") : qsTr("SecureVault — Unlock"))
    color: Theme.bg

    // Settings are only reachable while unlocked.
    property bool settingsOpen: false

    Connections {
        target: vault
        function onUnlockedChanged() {
            if (!vault.unlocked)
                window.settingsOpen = false
        }
    }

    LoginScreen {
        anchors.fill: parent
        visible: !vault.unlocked
        enabled: visible
    }

    VaultScreen {
        anchors.fill: parent
        visible: vault.unlocked && !window.settingsOpen
        enabled: visible
        onOpenSettings: window.settingsOpen = true
    }

    SettingsScreen {
        anchors.fill: parent
        visible: vault.unlocked && window.settingsOpen
        enabled: visible
        onBack: window.settingsOpen = false
    }
}
