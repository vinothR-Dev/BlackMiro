import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlackMiro

// "Link to": a web address, or another frame on this board.
Dialog {
    id: dlg

    required property BoardCanvas canvas
    property var frameList: []

    title: qsTr("Link to")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(440, parent ? parent.width - 32 : 440)
    padding: 20

    function apply() {
        let link = ""
        if (toFrame.checked && frameBox.currentIndex >= 0)
            link = "#" + frameList[frameBox.currentIndex].id
        else if (url.text.trim().length > 0) {
            link = url.text.trim()
            if (!/^[a-z][a-z0-9+.-]*:/i.test(link))
                link = "https://" + link
        }
        canvas.selLink = link
        close()
    }

    onOpened: {
        frameList = canvas.frames()
        const cur = canvas.selLink
        toFrame.checked = cur.startsWith("#")
        toUrl.checked = !toFrame.checked
        url.text = toFrame.checked ? "" : cur
        frameBox.currentIndex = Math.max(0, frameList.findIndex(f => "#" + f.id === cur))
        if (toUrl.checked) url.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: 10
        RadioButton { id: toUrl; text: qsTr("Web address"); checked: true }
        TextField {
            id: url
            Layout.fillWidth: true
            enabled: toUrl.checked
            placeholderText: "https://example.com"
            inputMethodHints: Qt.ImhUrlCharactersOnly
            onAccepted: dlg.apply()
        }
        RadioButton { id: toFrame; text: qsTr("Frame on this board"); enabled: dlg.frameList.length > 0 }
        ComboBox {
            id: frameBox
            Layout.fillWidth: true
            enabled: toFrame.checked
            model: dlg.frameList.map(f => f.name)
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.mutedText
            font.pixelSize: 12
            text: qsTr("A link icon appears next to the object. Click it to open the link.")
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight
        defaultButton: linkOk
        Button {
            text: qsTr("Remove link")
            visible: dlg.canvas.selLink.length > 0
            onClicked: { dlg.canvas.selLink = ""; dlg.close() }
        }
        Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        Button { id: linkOk; text: qsTr("Save"); onClicked: dlg.apply() }
    }
}
