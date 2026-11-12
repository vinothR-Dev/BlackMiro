import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// Share & live collaboration: start a session (host), join one with an invite link,
// see who's here, copy the link, leave. Also offers "share a copy" as a file.
Dialog {
    id: dlg

    required property CollabController collab
    required property CloudStore cloud
    required property BoardCanvas canvas
    // This board's share link (made on this device), refreshed on open and after saving.
    property string shareLink: ""
    onAboutToShow: shareLink = cloud.linkFor(canvas)
    Connections {
        target: dlg.cloud
        function onLinkReady(link) { dlg.shareLink = link }
    }
    property bool joinOnly: false            // opened from "Join live session"
    signal saveCopyRequested()
    signal exportRequested()

    title: dlg.joinOnly ? qsTr("Open a link") : qsTr("Share board")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, parent ? parent.width - 32 : 560)

    // Share: generate the invite link straight away (starts the session) and
    // copy it, so it is ready to paste. "New link" makes a fresh one.
    property bool autoCopy: false
    onOpened: {
        joinField.text = ""
        joinError.text = ""
        if (!dlg.joinOnly && dlg.collab.canHost && dlg.collab.state === "off") {
            dlg.autoCopy = true
            dlg.collab.startHosting()
        }
    }
    function newLink() {
        dlg.autoCopy = true
        dlg.collab.leave()
        dlg.collab.startHosting()
    }
    Connections {
        target: dlg.collab
        function onStateChanged() {
            if (dlg.autoCopy && dlg.collab.isHost && dlg.collab.inviteLink.length > 0) {
                dlg.autoCopy = false
                dlg.collab.copyInvite()
                copiedNote.visible = true
            }
            if (dlg.collab.state === "off" || dlg.collab.state === "error") copiedNote.visible = false
        }
    }

    readonly property bool live: dlg.collab.state === "live"
    readonly property bool busy: dlg.collab.state === "connecting"

    component Avatar: Rectangle {
        property string name
        property color tint
        implicitWidth: 32; implicitHeight: 32; radius: 16
        color: tint
        border.width: 2
        border.color: Theme.surface
        Label {
            anchors.centerIn: parent
            text: parent.name.length > 0 ? parent.name.charAt(0).toUpperCase() : "?"
            color: "white"
            font.weight: Font.DemiBold
        }
        ToolTip.visible: avatarHover.hovered
        ToolTip.text: name
        HoverHandler { id: avatarHover }
    }

    contentItem: ColumnLayout {
        spacing: 14

        // ---------------- Share link (encrypted copy on GitHub)
        Rectangle {
            visible: !dlg.joinOnly
            Layout.fillWidth: true
            implicitHeight: linkCol.implicitHeight + 28
            radius: 12
            color: Theme.surfaceAlt
            ColumnLayout {
                id: linkCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                spacing: 8
                Label { text: qsTr("Share link"); font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.text }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                    color: Theme.subText
                    text: qsTr("Anyone with the link can open a copy of this board, in the browser or the app. No account needed. The board is encrypted before it leaves this device; the key is only inside the link.")
                }
                Label {
                    visible: !dlg.cloud.available
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.mutedText
                    text: qsTr("Share links aren't available in this build. Use Download board below instead.")
                }
                RowLayout {
                    visible: dlg.cloud.available
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        highlighted: dlg.shareLink.length === 0
                        enabled: !dlg.cloud.busy
                        text: dlg.cloud.busy ? qsTr("Saving…") : dlg.shareLink.length > 0 ? qsTr("Update link") : qsTr("Create share link")
                        onClicked: dlg.cloud.saveBoard(dlg.canvas)
                    }
                    BusyIndicator { visible: dlg.cloud.busy; running: visible; implicitWidth: 28; implicitHeight: 28 }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    visible: dlg.shareLink.length > 0
                    Layout.fillWidth: true
                    spacing: 8
                    TextField { Layout.fillWidth: true; readOnly: true; selectByMouse: true; text: dlg.shareLink }
                    Button { highlighted: true; text: qsTr("Copy"); onClicked: dlg.cloud.copyLink(dlg.shareLink) }
                }
            }
        }

        // ---------------- Not in a session
        ColumnLayout {
            visible: dlg.collab.state === "off" || dlg.collab.state === "error"
            Layout.fillWidth: true
            spacing: 12

            Label {
                visible: dlg.collab.state === "error"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.danger
                text: dlg.collab.statusText
            }

            // Host
            Rectangle {
                visible: !dlg.joinOnly
                Layout.fillWidth: true
                implicitHeight: hostCol.implicitHeight + 28
                radius: 12
                color: Theme.surfaceAlt
                ColumnLayout {
                    id: hostCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                    spacing: 6
                    Label { text: qsTr("Edit together, live"); font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.text }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                        color: Theme.subText
                        text: dlg.collab.canHost
                              ? qsTr("Start a session and send the invite link. People on your network open it in Miru (desktop, phone or browser) and edit with you. Everything is end-to-end encrypted with the key inside the link.")
                              : qsTr("The browser version can join sessions but can't host one. Start the session from Miru on a computer or phone.")
                    }
                    Button {
                        Layout.topMargin: 6
                        visible: dlg.collab.canHost
                        highlighted: true
                        text: qsTr("Start live session")
                        onClicked: {
                            if (dlg.collab.state === "error") dlg.collab.leave()
                            dlg.collab.startHosting()
                        }
                    }
                }
            }

            // Join
            Label { text: qsTr("Have a link?"); font.weight: Font.DemiBold; color: Theme.text; Layout.topMargin: dlg.joinOnly ? 0 : 4 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: joinField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Share link or live-session invite")
                    onAccepted: joinBtn.clicked()
                }
                Button {
                    id: joinBtn
                    text: qsTr("Join")
                    highlighted: dlg.joinOnly
                    enabled: joinField.text.trim().length > 0
                    onClicked: {
                        if (dlg.cloud.isShareLink(joinField.text)) {
                            joinError.text = ""
                            dlg.cloud.openLink(dlg.canvas, joinField.text)
                            dlg.close()
                            return
                        }
                        if (!dlg.collab.isInviteLink(joinField.text)) {
                            joinError.text = qsTr("That doesn't look like a Miru link.")
                            return
                        }
                        joinError.text = ""
                        if (dlg.collab.state === "error") dlg.collab.leave()
                        dlg.collab.join(joinField.text)
                    }
                }
            }
            Label {
                id: joinError
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.danger
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.mutedText
                font.pixelSize: 12
                text: qsTr("Joining replaces what's on your screen with the shared board — save your current board first if you need it.")
            }

            // Share a copy
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; visible: !dlg.joinOnly; Layout.topMargin: 4 }
            RowLayout {
                visible: !dlg.joinOnly
                Layout.fillWidth: true
                spacing: 8
                Label { text: qsTr("Or share a copy:"); color: Theme.subText; Layout.fillWidth: true }
                Button { text: Qt.platform.os === "wasm" ? qsTr("Download board") : qsTr("Save a copy"); onClicked: { dlg.close(); dlg.saveCopyRequested() } }
                Button { text: qsTr("Export…"); onClicked: { dlg.close(); dlg.exportRequested() } }
            }
        }

        // ---------------- Connecting / live
        ColumnLayout {
            visible: dlg.live || dlg.busy
            Layout.fillWidth: true
            spacing: 12

            RowLayout {
                spacing: 8
                Rectangle {
                    implicitWidth: 10; implicitHeight: 10; radius: 5
                    color: dlg.live ? Theme.success : Theme.mutedText
                    SequentialAnimation on opacity {
                        running: dlg.busy
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 500 }
                        NumberAnimation { to: 1; duration: 500 }
                    }
                }
                Label { text: dlg.collab.statusText; color: Theme.text; font.weight: Font.Medium }
            }

            Label { visible: dlg.collab.isHost; text: qsTr("Invite link"); font.weight: Font.DemiBold; color: Theme.text }
            RowLayout {
                visible: dlg.collab.isHost
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    Layout.fillWidth: true
                    readOnly: true
                    text: dlg.collab.inviteLink
                    selectByMouse: true
                }
                Button { highlighted: true; text: qsTr("Copy"); onClicked: { dlg.collab.copyInvite(); copiedNote.visible = true } }
                Button {
                    text: qsTr("New link")
                    ToolTip.visible: hovered
                    ToolTip.delay: 450
                    ToolTip.text: qsTr("Make a fresh invite link. The old link stops working and people connected now are disconnected.")
                    onClicked: dlg.newLink()
                }
            }
            Label {
                id: copiedNote
                visible: false
                color: Theme.success
                font.pixelSize: 12
                text: qsTr("Link copied — paste it to anyone you want to invite.")
            }
            Label {
                visible: dlg.collab.isHost
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.mutedText
                font.pixelSize: 12
                text: qsTr("Anyone with this link can edit while your session is open. If others can't connect, allow Miru through your firewall when Windows asks.")
            }

            Label { text: qsTr("In this board"); font.weight: Font.DemiBold; color: Theme.text }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                RowLayout {
                    spacing: 8
                    Avatar { name: dlg.collab.displayName; tint: dlg.collab.myColor }
                    Label { text: qsTr("%1 (you)").arg(dlg.collab.displayName); color: Theme.text }
                }
                Repeater {
                    model: dlg.collab.peers
                    delegate: RowLayout {
                        required property var modelData
                        spacing: 8
                        Avatar { name: modelData.name; tint: modelData.color }
                        Label { text: modelData.name; color: Theme.text }
                    }
                }
            }
            Label {
                visible: dlg.live && dlg.collab.peers.length === 0
                color: Theme.mutedText
                text: dlg.collab.isHost ? qsTr("Waiting for people to join…") : qsTr("Waiting for the other people…")
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            visible: dlg.live || dlg.busy
            text: dlg.collab.isHost ? qsTr("End session") : qsTr("Leave")
            onClicked: dlg.collab.leave()
        }
        Button {
            id: closeBtn; highlighted: true
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
