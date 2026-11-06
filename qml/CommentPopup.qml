import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlackMiro

// Card next to a comment pin. Saved when it closes; an empty comment is removed.
Popup {
    id: pop

    required property BoardCanvas canvas
    property string commentId: ""
    property var info: ({})

    function openFor(id, x, y) {
        if (opened && commentId === id)
            return
        if (opened)
            close()
        commentId = id
        info = canvas.commentInfo(id)
        field.text = info.text || ""
        pop.x = Math.max(8, Math.min(x + 8, parent.width - width - 8))
        pop.y = Math.max(8, Math.min(y - 8, parent.height - height - 8))
        open()
        field.forceActiveFocus()
        field.cursorPosition = field.length
    }

    width: 300
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: {
        canvas.setCommentText(commentId, field.text)
        canvas.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            spacing: 8
            Rectangle {
                implicitWidth: 28; implicitHeight: 28; radius: 14
                color: "#ffd02f"
                Label {
                    anchors.centerIn: parent
                    text: (pop.info.author || "?").charAt(0).toUpperCase()
                    font.bold: true
                    color: "#282828"
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label { Layout.fillWidth: true; text: pop.info.author || qsTr("Guest"); color: Theme.text; font.bold: true; elide: Text.ElideRight }
                Label { text: pop.info.time || ""; color: Theme.mutedText; font.pixelSize: 12; visible: text.length > 0 }
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(160, Math.max(70, field.implicitHeight))
            TextArea {
                id: field
                placeholderText: qsTr("Write a comment…")
                wrapMode: TextEdit.Wrap
                Keys.onPressed: (e) => {
                    if ((e.key === Qt.Key_Return || e.key === Qt.Key_Enter) && (e.modifiers & Qt.ControlModifier)) {
                        pop.close()
                        e.accepted = true
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Delete")
                flat: true
                onClicked: { field.text = ""; pop.close() }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Save")
                highlighted: true
                onClicked: pop.close()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Ctrl+Enter")
            }
        }
    }
}
