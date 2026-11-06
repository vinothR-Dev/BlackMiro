import QtQuick
import QtQuick.Controls
import BlackMiro

// Cursor chat: a short message shown next to your cursor to everyone in the live session.
Rectangle {
    id: chat

    required property BoardCanvas canvas
    property color tint: Theme.accent

    function start(px, py) {
        x = Math.max(8, Math.min(px + 14, parent.width - width - 8))
        y = Math.max(8, Math.min(py + 18, parent.height - height - 8))
        field.text = ""
        visible = true
        field.forceActiveFocus()
    }
    function finish(keep) {
        if (keep && field.text.trim().length > 0) {
            clearTimer.restart()      // message stays a few seconds, then disappears
        } else {
            canvas.cursorChat = ""
        }
        visible = false
        canvas.forceActiveFocus()
    }

    visible: false
    width: 260
    height: field.implicitHeight + 8
    radius: height / 2
    color: tint
    z: 40

    TextField {
        id: field
        anchors.fill: parent
        anchors.margins: 4
        color: "white"
        placeholderText: qsTr("Say something… (Enter to send)")
        placeholderTextColor: Qt.rgba(1, 1, 1, 0.7)
        maximumLength: 140
        background: null
        onTextChanged: chat.canvas.cursorChat = text
        onAccepted: chat.finish(true)
        Keys.onEscapePressed: chat.finish(false)
        onActiveFocusChanged: if (!activeFocus && chat.visible) chat.finish(true)
    }

    Timer {
        id: clearTimer
        interval: 5000
        onTriggered: chat.canvas.cursorChat = ""
    }
}
