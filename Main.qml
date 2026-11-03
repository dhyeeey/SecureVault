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
        onBack: {
            if (!vault.busy)
                window.settingsOpen = false
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: vault.unlocked && vault.busy
        color: "#40000000"
        z: 50

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.centerIn: parent
            width: 360
            height: busyColumn.implicitHeight + 40
            radius: 10
            color: Theme.panel
            border.color: Theme.line

            Column {
                id: busyColumn
                anchors.centerIn: parent
                width: parent.width - 40
                spacing: 12

                Label {
                    width: parent.width
                    text: vault.progressText.length ? vault.progressText : qsTr("Working…")
                    elide: Text.ElideMiddle
                    color: Theme.text
                    font.pixelSize: 14
                    font.bold: true
                }
                ProgressBar {
                    id: bar
                    width: parent.width
                    value: vault.progress
                    background: Rectangle {
                        implicitHeight: 6
                        radius: 3
                        color: Theme.line
                    }
                    contentItem: Item {
                        implicitHeight: 6
                        Rectangle {
                            width: bar.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: Theme.accent
                        }
                    }
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: qsTr("Keep the app open until this finishes.")
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }
        }
    }
}
