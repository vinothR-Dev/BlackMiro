import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// One dialog for every password question:
//   mode "set"   - choose a password (with confirmation) when making a board private
//   mode "enter" - type the board password (save, export, open, make public)
Dialog {
    id: dlg

    property string mode: "enter"
    property string message: ""
    property string error: ""
    property string actionText: "OK"
    signal submitted(string password)

    function ask(title, message, actionText, mode) {
        dlg.title = title
        dlg.message = message
        dlg.actionText = actionText
        dlg.mode = mode || "enter"
        dlg.error = ""
        pw.text = ""
        confirm.text = ""
        open()
        pw.forceActiveFocus()
    }

    function showError(text) {
        error = text
        pw.selectAll()
        pw.forceActiveFocus()
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, parent ? parent.width - 32 : 420)
    padding: 20
    closePolicy: Popup.CloseOnEscape

    readonly property bool valid: mode === "set" ? pw.text.length >= 4 && pw.text === confirm.text
                                                 : pw.text.length > 0

    function accept_() {
        if (!valid)
            return
        dlg.submitted(pw.text)
    }

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            visible: dlg.message.length > 0
            wrapMode: Text.WordWrap
            color: Theme.subText
            text: dlg.message
        }
        TextField {
            id: pw
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: dlg.mode === "set" ? "New password (at least 4 characters)" : "Password"
            onAccepted: dlg.mode === "set" ? confirm.forceActiveFocus() : dlg.accept_()
        }
        TextField {
            id: confirm
            visible: dlg.mode === "set"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: "Repeat password"
            onAccepted: dlg.accept_()
        }
        Label {
            Layout.fillWidth: true
            visible: dlg.mode === "set" && confirm.text.length > 0 && pw.text !== confirm.text
            color: Theme.danger
            text: "The passwords don't match."
        }
        Label {
            Layout.fillWidth: true
            visible: dlg.error.length > 0
            wrapMode: Text.WordWrap
            color: Theme.danger
            text: dlg.error
        }
        Label {
            Layout.fillWidth: true
            visible: dlg.mode === "set"
            wrapMode: Text.WordWrap
            color: Theme.mutedText
            font.pixelSize: 12
            text: "There is no password recovery: a forgotten password means the encrypted file can't be opened."
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            id: pwGo
            text: dlg.actionText
            highlighted: true
            enabled: dlg.valid
            onClicked: dlg.accept_()
        }
    }
}
