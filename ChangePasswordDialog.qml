import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("Change password")
    okText: qsTr("Change password")

    readonly property bool mismatch: confirmPw.text.length > 0 && confirmPw.text !== newPw.text
    okEnabled: oldPw.text.length > 0 && newPw.text.length >= 8 && confirmPw.text === newPw.text

    function clearFields() {
        oldPw.text = ""
        newPw.text = ""
        confirmPw.text = ""
    }

    onOpened: {
        clearFields()
        oldPw.forceActiveFocus()
    }
    onAccepted: {
        vault.changePassword(oldPw.text, newPw.text)
        clearFields()
    }
    onRejected: clearFields()

    ColumnLayout {
        width: 360
        spacing: 10

        AppTextField {
            id: oldPw
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Current password")
        }
        AppTextField {
            id: newPw
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("New password")
        }
        AppTextField {
            id: confirmPw
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Confirm new password")
            invalid: dialog.mismatch
            Keys.onReturnPressed: if (dialog.okEnabled) dialog.accept()
            Keys.onEnterPressed: if (dialog.okEnabled) dialog.accept()
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: dialog.mismatch ? Theme.danger : Theme.muted
            text: dialog.mismatch
                  ? qsTr("Passwords do not match.")
                  : qsTr("At least 8 characters. Your files are not re-encrypted; only the key wrapper changes.")
        }
    }
}
