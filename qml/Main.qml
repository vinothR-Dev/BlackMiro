import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Miru

ApplicationWindow {
    id: win

    width: 1440
    height: 900
    visible: true
    title: (canvas.modified ? "• " : "") + canvas.boardTitle + (canvas.boardPrivate ? " (private)" : "") + " — Miru"
    color: Theme.canvas

    // Basic-style controls (fields, menus, dialogs, check boxes) follow the theme.
    palette.window: Theme.surface
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceAlt
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.brightText: Theme.accentText
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.placeholderText: Theme.mutedText
    palette.light: Theme.surface
    palette.midlight: Theme.hover
    palette.mid: Theme.border
    palette.dark: Theme.pressed
    palette.shadow: Theme.dark ? "#000000" : "#a0a4ab"
    palette.toolTipBase: Theme.toast
    palette.toolTipText: "#ffffff"

    property bool closeConfirmed: false
    property bool closeAfterSave: false
    property string pendingPassword: ""      // private board: password for the save in progress
    // The web build has no file system: it uses the browser's open / download dialogs.
    readonly property bool isWeb: Qt.platform.os === "wasm"

    property bool showHome: true
    readonly property alias collabRef: collab

    function openPath(url) { showHome = false; canvas.openFile(url) }
    function openPaths(urls) {
        showHome = false
        // Wait one frame so the canvas has its real size before placing images.
        Qt.callLater(() => canvas.addImagesFromUrls(urls, -1, -1))
    }
    function openExport() { exportDialog.open() }
    function openSignIn() { save() }
    function openShare() { share() }
    // Screenshot hooks (--call): open each tool panel.
    function showShapesMenu() { activateTool("shapes", null) }
    function showEmojiMenu() { activateTool("emoji", null) }
    function showShapeLibrary() { leftPanel = "shapes" }
    function showIcons() { leftPanel = "icons" }
    function showLayers() { canvas.addLayer("Notes"); layersOpen = true }
    function showTools() { plusBtn.clicked() }
    function showTemplates() { templatesDialog.open() }
    function showPen() { canvas.tool = "pen" }
    function showSticky() { canvas.tool = "sticky" }
    function showFrame() { canvas.tool = "frame" }
    // Automated collaboration test: one visible edit that must reach the other window.
    function demoRecolor() {
        canvas.selectAll()
        canvas.selFill = "#93d275"
        canvas.boardTitle = "Edited live by host"
        canvas.clearSelection()
    }
    function openMakePrivate() { togglePrivate() }
    function openContextMenu() { canvas.selectAll(); ctxMenu.popup(canvas, 420, 300) }
    function openBoardMenu() { showHome = false; canvas.clearSelection(); ctxMenu.popup(canvas, 420, 120) }
    function openShortcuts() { showHome = false; shortcutsDialog.open() }
    function demoComment() {
        showHome = false
        canvas.addStickyAt(500, 360); canvas.endTextEdit()
        canvas.addCommentAt(-1, -1)
    }
    function demoGaps() {
        showHome = false
        canvas.addStickyAt(300, 400); canvas.endTextEdit()
        canvas.addStickyAt(900, 400); canvas.endTextEdit()
        canvas.addStickyAt(600, 140); canvas.endTextEdit()
        canvas.addStickyAt(600, 420); canvas.endTextEdit()
        canvas.clearSelection()
        canvas.simulateDrag(Qt.point(600, 420), Qt.point(603, 445))
    }
    function demoArrange() {
        showHome = false
        canvas.addStickyAt(300, 300); canvas.endTextEdit()
        canvas.addStickyAt(700, 520); canvas.endTextEdit()
        canvas.addStickyAt(520, 700); canvas.endTextEdit()
        canvas.selectAll()
        canvas.arrangeSelection(false)
    }
    function demoExport() { showHome = false; exportDialog.open() }
    function openAbout() { aboutDialog.open() }
    function demoShare() { showHome = false; shareDialog.joinOnly = false; shareDialog.open() }
    function demoConnectDots() {
        showHome = false
        canvas.gridStyle = "lines"
        canvas.addStickyAt(520, 420); canvas.endTextEdit()
    }
    function demoConnector() {
        showHome = false
        canvas.addStickyAt(380, 420); canvas.endTextEdit()
        const p = canvas.connectDotScreen(1)
        canvas.simulateDrag(p, p); canvas.simulateRelease(p)          // adds a connected copy
        canvas.selectConnectorDemo()
    }
    function demoInfoMenu() {
        showHome = false
        canvas.addStickyAt(520, 380); canvas.endTextEdit()
        canvas.selectAll()
        canvas.showObjectSize = true
        canvas.selLink = "https://miro.com"
        ctxMenu.popup(canvas, 700, 200)
        Qt.callLater(() => ctxMenu.openInfo())
    }

    function openBoard() {
        if (isWeb) canvas.browserOpenBoard()
        else openDialog.open()
    }
    function insertImages() {
        if (isWeb) canvas.browserInsertImages()
        else imageDialog.open()
    }

    // ---- Saving (private boards ask for their password every time)
    function save() { requireSignIn(qsTr("Sign in with your email to save this board. No password needed."),
                                    () => withPassword("save", () => doSave(false))) }
    function saveAs() { requireSignIn(qsTr("Sign in with your email to save this board. No password needed."),
                                      () => withPassword("save", () => doSave(true))) }
    function share() { requireSignIn(qsTr("Sign in with your email to share this board and collaborate."),
                                     () => { shareDialog.joinOnly = false; shareDialog.open() }) }
    function joinSession() { shareDialog.joinOnly = true; shareDialog.open() }

    // Using the board needs no account. Saving and sharing do: sign in first, then continue.
    property var afterSignIn: null
    function requireSignIn(reason, action) {
        if (AppState.signedIn) {
            action()
            return
        }
        afterSignIn = action
        signIn.reason = reason
        signIn.visible = true
    }
    Connections {
        target: AppState
        function onProfileChanged() {
            if (AppState.signedIn && signIn.visible) {
                signIn.visible = false
                const action = win.afterSignIn
                win.afterSignIn = null
                if (action) Qt.callLater(action)
            }
        }
    }
    function doSave(asNew) {
        if (isWeb)
            canvas.browserSaveBoard(pendingPassword)
        else if (!asNew && canvas.filePath.length > 0) {
            canvas.saveFile("", pendingPassword)
            afterSave()
        } else {
            saveDialog.open()
            return
        }
        pendingPassword = ""
    }
    function afterSave() {
        pendingPassword = ""
        if (closeAfterSave && !canvas.modified) {
            closeConfirmed = true
            win.close()
        }
        closeAfterSave = false
    }

    // Runs `action` directly for normal boards; for private boards asks the password first.
    property var passwordAction: null
    property string passwordPurpose: ""
    function withPassword(purpose, action) {
        if (!canvas.boardPrivate) {
            pendingPassword = ""
            action()
            return
        }
        passwordPurpose = purpose
        passwordAction = action
        pwDialog.ask("Save private board",
                     "\"" + canvas.boardTitle + "\" is private. Enter its password to save it encrypted.",
                     "Save", "enter")
    }

    function togglePrivate() {
        passwordAction = null
        if (canvas.boardPrivate) {
            passwordPurpose = "makePublic"
            pwDialog.ask("Make board public",
                         "Enter the board password. Saved files will no longer be encrypted.", "Make public", "enter")
        } else {
            passwordPurpose = "makePrivate"
            pwDialog.ask("Make board private",
                         "Choose a password. Saving this board and exporting PDFs will ask for it, "
                         + "and the files are encrypted with it.", "Make private", "set")
        }
    }

    onClosing: (close) => {
        if (canvas.modified && !closeConfirmed && !isWeb) {
            close.accepted = false
            unsavedDialog.open()
        }
    }

    // ------------------------------------------------------------------ canvas
    BoardCanvas {
        id: canvas
        anchors.fill: parent
        focus: true
        fillColor: Theme.canvas
        gridColor: Theme.grid
        ownerEmail: AppState.email
        onMessage: (text) => toast.show(text)
        onContextMenuRequested: (x, y) => ctxMenu.popup(canvas, x, y)
        onPasswordRequired: (name) => {
            win.passwordPurpose = "open"
            pwDialog.ask("Private board", "\"" + name + "\" is private. Enter its password to open it.", "Open", "enter")
        }
        Component.onCompleted: forceActiveFocus()
    }

    // Encrypted boards on a GitHub branch + share links (see CloudStore.h).
    CloudStore {
        id: cloud
        userEmail: AppState.email
        onMessage: (text) => toast.show(text)
        onFailed: (text) => toast.show(text)
        onLinkReady: (link) => { cloud.copyLink(link) }
        onOpened: (title) => { win.showHome = false; toast.show(qsTr("Opened \"%1\"").arg(title)) }
        Component.onCompleted: {
            const link = startupLink()        // web: page opened from a share link
            if (link.length > 0) {
                win.showHome = false
                openLink(canvas, link)
            }
        }
    }

    CollabController {
        id: collab
        canvas: canvas
        displayName: AppState.signedIn ? AppState.email.split("@")[0] : qsTr("Guest")
        onMessage: (text) => toast.show(text)
        onStateChanged: if (state === "live") win.showHome = false
    }

    Connections {
        target: canvas
        function onBoardSaved(path, title) { AppState.addRecent(path, title, canvas.saveThumbnail()) }
        function onLinkActivated(url) {
            if (url === "miru:edit-link") {
                if (canvas.selectionCount > 0) linkDialog.open()
            } else if (url === "miru:cursor-chat") {
                cursorChat.start(win.pointer.x, win.pointer.y)
            } else if (/^(https?|mailto):/i.test(url)) {
                Qt.openUrlExternally(url)
            } else {
                toast.show(qsTr("Only web and e-mail links can be opened"))
            }
        }
        function onCommentOpened(id, x, y) { commentPopup.openFor(id, x, y) }
        function onShortcutsRequested() { shortcutsDialog.open() }
        function onShapesRequested() { win.activateTool("shapes", null) }
        function onToolSlotRequested(index) {
            const order = ["select"].concat(AppState.pinnedTools)
            if (index - 1 >= order.length) return
            const id = order[index - 1]
            if (id === "select") canvas.tool = "select"
            else win.activateTool(id, null)
        }
    }
    // Last pointer position on screen (cursor chat starts there).
    property point pointer: Qt.point(width / 2, height / 2)
    HoverHandler {
        target: canvas
        onPointChanged: win.pointer = point.position
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (!drop.hasUrls) return
            // Several files land side by side in one horizontal row at the drop point.
            canvas.addImagesFromUrls(drop.urls, drop.x, drop.y)
            drop.acceptProposedAction()
        }
    }

    // ------------------------------------------------------------------ text editor overlay
    property bool wasEditing: false
    Connections {
        target: canvas
        function onEditingChanged() {
            if (canvas.editing && !win.wasEditing) {
                editor.text = canvas.editText
                editor.forceActiveFocus()
                editor.cursorPosition = editor.length
                editor.selectAll()
            }
            win.wasEditing = canvas.editing
        }
    }

    TextArea {
        id: editor
        visible: canvas.editing
        x: canvas.editRect.x
        y: canvas.editRect.y
        width: Math.max(20, canvas.editRect.width)
        height: Math.max(canvas.editRect.height, contentHeight)
        padding: 0
        wrapMode: TextEdit.Wrap
        background: null
        color: canvas.editColor
        selectionColor: Theme.selection
        selectedTextColor: canvas.editColor
        font.pixelSize: Math.max(1, Math.round(canvas.editFontPixelSize))
        font.bold: canvas.editBold
        horizontalAlignment: canvas.editAlign === 1 ? TextEdit.AlignLeft
                           : canvas.editAlign === 2 ? TextEdit.AlignRight : TextEdit.AlignHCenter
        verticalAlignment: canvas.editCenterV ? TextEdit.AlignVCenter : TextEdit.AlignTop
        onTextChanged: if (canvas.editing) canvas.updateEditText(text)
        onActiveFocusChanged: if (!activeFocus && canvas.editing) canvas.endTextEdit()
        Keys.onEscapePressed: { canvas.endTextEdit(); canvas.forceActiveFocus() }
        Keys.onPressed: (e) => {
            if ((e.key === Qt.Key_Return || e.key === Qt.Key_Enter) && (e.modifiers & Qt.ControlModifier)) {
                canvas.endTextEdit(); canvas.forceActiveFocus(); e.accepted = true
            }
        }
    }

    // ------------------------------------------------------------------ chrome
    component Card: Rectangle {
        radius: 12
        color: Theme.surface
        border.color: Theme.border
        Rectangle {   // soft shadow under floating bars
            z: -1
            anchors.fill: parent
            anchors.topMargin: 3
            anchors.margins: -1
            radius: parent.radius + 1
            color: Qt.alpha(Theme.dark ? "#000000" : "#1f2a44", Theme.dark ? 0.35 : 0.07)
        }
    }

    // Top-left: menu, title, private lock, undo/redo
    Card {
        id: topLeft
        x: 12; y: 12
        height: 48
        width: topRow.implicitWidth + 12
        RowLayout {
            id: topRow
            anchors.verticalCenter: parent.verticalCenter
            x: 6
            spacing: 2
            ToolButtonIcon {
                tip: "Menu"
                iconPath: "M4 7 H20 M4 12 H20 M4 17 H20"
                onClicked: mainMenu.popup(this, 0, height + 6)
            }
            // Wordmark (like Miro's logo in the board bar); click goes to Home.
            ToolButton {
                id: brand
                visible: win.width >= 600
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                implicitHeight: 40
                leftPadding: 6; rightPadding: 8
                ToolTip.visible: hovered
                ToolTip.delay: 450
                ToolTip.text: qsTr("Go to Home")
                Accessible.name: qsTr("Miru, go to Home")
                onClicked: win.showHome = true
                background: Rectangle { radius: 8; color: brand.hovered ? Theme.hover : "transparent" }
                contentItem: Row {
                    spacing: 4
                    MiruLogo { width: 28; height: 28; anchors.verticalCenter: parent.verticalCenter }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Miru"
                        font.pixelSize: 19
                        font.weight: Font.Bold
                        font.letterSpacing: -0.4
                        color: Theme.text
                    }
                }
            }
            Rectangle { visible: brand.visible; Layout.preferredWidth: 1; Layout.preferredHeight: 22; color: Theme.border }
            TextField {
                id: titleField
                text: canvas.boardTitle
                implicitWidth: Math.max(80, Math.min(win.width < 600 ? 96 : 320, contentWidth + 24))
                font.pixelSize: 15
                font.bold: true
                color: Theme.text
                background: Rectangle { radius: 6; color: titleField.activeFocus ? Theme.hover : "transparent" }
                onEditingFinished: { canvas.boardTitle = text; cursorPosition = 0; canvas.forceActiveFocus() }
                onTextChanged: if (!activeFocus) cursorPosition = 0   // show the start, not the end
            }
            ToolButtonIcon {
                tip: canvas.boardPrivate ? "Private board — click to make public"
                                         : "Make this board private (password for save and PDF export)"
                active: canvas.boardPrivate
                iconPath: canvas.boardPrivate ? "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 7 0 V11"
                                              : "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 6.8 -1.2"
                onClicked: win.togglePrivate()
            }
            Rectangle { implicitWidth: 1; implicitHeight: 24; color: Theme.border }
            ToolButtonIcon {
                tip: "Undo (Ctrl+Z)"
                enabled: canvas.canUndo
                iconPath: "M9 7 L5 11 L9 15 M5 11 H15 a4 4 0 0 1 0 8 H11"
                onClicked: canvas.undo()
            }
            ToolButtonIcon {
                tip: "Redo (Ctrl+Y)"
                enabled: canvas.canRedo
                iconPath: "M15 7 L19 11 L15 15 M19 11 H9 a4 4 0 0 0 0 8 H13"
                onClicked: canvas.redo()
            }
        }
    }

    Menu {
        id: mainMenu
        MenuItem { text: qsTr("Home"); onTriggered: win.showHome = true }
        MenuItem { text: "New board\tCtrl+N"; onTriggered: canvas.newBoard() }
        MenuItem { text: "Open…\tCtrl+O"; onTriggered: win.openBoard() }
        MenuItem { text: win.isWeb ? "Save (download)\tCtrl+S" : "Save\tCtrl+S"; onTriggered: win.save() }
        MenuItem { text: win.isWeb ? "Download .board file" : "Save as…\tCtrl+Shift+S"; onTriggered: win.saveAs() }
        MenuSeparator {}
        MenuItem { text: "Insert image…\tCtrl+I"; onTriggered: win.insertImages() }
        MenuItem { text: "Export…\tCtrl+E"; onTriggered: exportDialog.open() }
        MenuItem {
            text: canvas.boardPrivate ? "Make board public…" : "Make board private…"
            onTriggered: win.togglePrivate()
        }
        MenuSeparator {}
        Menu {
            id: versionsMenu
            title: "Version history"
            enabled: canvas.versionLabels.length > 0
            Instantiator {
                model: canvas.versionLabels
                delegate: MenuItem {
                    required property int index
                    required property string modelData
                    text: (index === 0 ? "Latest save · " : "") + modelData
                    onTriggered: canvas.restoreVersion(index)
                }
                onObjectAdded: (index, object) => versionsMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => versionsMenu.removeItem(object)
            }
        }
        MenuItem {
            text: qsTr("Show grid")
            checkable: true
            checked: canvas.showGrid
            onTriggered: canvas.showGrid = checked
        }
        MenuItem {
            text: qsTr("Dot grid")
            checkable: true
            checked: canvas.gridStyle === "dots"
            onTriggered: { canvas.gridStyle = "dots"; canvas.showGrid = true }
        }
        MenuItem {
            text: qsTr("Line grid")
            checkable: true
            checked: canvas.gridStyle === "lines"
            onTriggered: { canvas.gridStyle = "lines"; canvas.showGrid = true }
        }
        MenuSeparator {}
        MenuItem { text: qsTr("Layers"); onTriggered: win.layersOpen = !win.layersOpen }
        MenuItem { text: qsTr("Templates…"); onTriggered: templatesDialog.open() }
        MenuItem { text: qsTr("Share…"); onTriggered: win.share() }
        MenuItem { text: qsTr("Open share link…"); onTriggered: win.joinSession() }
        MenuItem { text: qsTr("Join live session…"); onTriggered: win.joinSession() }
        MenuSeparator {}
        MenuItem { text: qsTr("About Miru"); onTriggered: aboutDialog.open() }
    }

    // Top-right: theme + export
    Card {
        anchors { top: parent.top; right: parent.right; margins: 12 }
        height: 48
        width: rightRow.implicitWidth + 12
        RowLayout {
            id: rightRow
            anchors.centerIn: parent
            spacing: 4
            // Live session: who's here (click to manage the session)
            Rectangle {
                visible: collab.state === "live" || collab.state === "connecting"
                implicitHeight: 34
                implicitWidth: liveRow.implicitWidth + 16
                radius: 17
                color: Qt.alpha(Theme.success, 0.14)
                RowLayout {
                    id: liveRow
                    anchors.centerIn: parent
                    spacing: -6
                    Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: Theme.success; Layout.rightMargin: 12 }
                    Repeater {
                        model: collab.peers
                        delegate: Rectangle {
                            required property var modelData
                            implicitWidth: 26; implicitHeight: 26; radius: 13
                            color: modelData.color
                            border.width: 2
                            border.color: Theme.surface
                            Label {
                                anchors.centerIn: parent
                                text: modelData.name.charAt(0).toUpperCase()
                                color: "white"; font.pixelSize: 11; font.weight: Font.DemiBold
                            }
                        }
                    }
                    Label {
                        Layout.leftMargin: 10
                        text: collab.peers.length > 0 ? qsTr("Live · %1").arg(collab.peers.length + 1) : qsTr("Live")
                        color: Theme.success
                        font.weight: Font.DemiBold
                    }
                }
                TapHandler { onTapped: { shareDialog.joinOnly = false; shareDialog.open() } }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
            ThemeButton {}
            // Account: avatar when signed in, otherwise "Sign in"
            ToolButton {
                id: accountBtn
                visible: AppState.signedIn
                focusPolicy: Qt.NoFocus
                implicitWidth: 40
                implicitHeight: 40
                ToolTip.visible: hovered && !accountMenu.visible
                ToolTip.text: AppState.email
                onClicked: accountMenu.popup(accountBtn, accountBtn.width - accountMenu.width, accountBtn.height + 6)
                contentItem: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: 30; height: 30; radius: 15
                        color: Theme.accent
                        Label {
                            anchors.centerIn: parent
                            text: AppState.email.length > 0 ? AppState.email[0].toUpperCase() : ""
                            color: Theme.accentText
                            font.weight: Font.DemiBold
                        }
                    }
                }
                Menu {
                    id: accountMenu
                    MenuItem { text: qsTr("Signed in as %1").arg(AppState.email); enabled: false }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Sign out"); onTriggered: AppState.signOut() }
                }
            }
            Button {
                visible: !AppState.signedIn
                flat: true
                focusPolicy: Qt.NoFocus
                text: qsTr("Sign in")
                onClicked: win.requireSignIn("", () => {})
            }
            Button {
                focusPolicy: Qt.NoFocus
                text: qsTr("Share")
                onClicked: win.share()
            }
            Button {
                highlighted: true
                focusPolicy: Qt.NoFocus
                text: qsTr("Export")
                onClicked: exportDialog.open()
            }
        }
    }

    // Left: tools — Select, then the tools pinned in the "+" panel, then "+"
    readonly property var shapeTools: ["rectangle", "rounded", "ellipse", "diamond", "triangle", "divider", "line", "arrow", "elbow", "connector"]
    property string leftPanel: ""          // "" | shapes | icons
    property bool layersOpen: false

    function toolActive(id) {
        const t = canvas.tool
        switch (id) {
        case "sticky": case "text": case "frame": case "hand": case "lasso": case "comment": return t === id
        case "pen": return ["pen", "highlighter", "smartpen", "eraser", "precision-eraser", "lasso"].indexOf(t) >= 0
        case "eraser": return t === "eraser" || t === "precision-eraser"
        case "connector": return ["connector", "arrow", "line", "elbow"].indexOf(t) >= 0
        case "shapes": return shapeTools.indexOf(t) >= 0 || t.startsWith("shape:")
        case "shapelib": return leftPanel === "shapes"
        case "icons": return leftPanel === "icons" || t.startsWith("icon:")
        case "emoji": return t.startsWith("emoji:")
        case "layers": return layersOpen
        default: return false
        }
    }

    function activateTool(id, anchor) {
        switch (id) {
        case "templates": templatesDialog.open(); return
        case "sticky": case "text": case "frame": case "hand": case "lasso": case "eraser": canvas.tool = id; break
        case "pen": canvas.tool = "pen"; break
        case "connector": canvas.tool = "arrow"; break
        case "shapes": case "emoji":
            flyout.kind = id
            const p = anchor ? anchor.mapToItem(win.contentItem, 0, 0) : Qt.point(0, 120)
            flyout.x = toolbar.x + toolbar.width + 8
            flyout.y = Math.max(8, Math.min(p.y - 8, win.height - flyout.implicitHeight - 16))
            flyout.open()
            return
        case "image": win.insertImages(); return
        case "icons": leftPanel = leftPanel === "icons" ? "" : "icons"; return
        case "shapelib": leftPanel = leftPanel === "shapes" ? "" : "shapes"; return
        case "layers": layersOpen = !layersOpen; return
        case "bulk": bulkDialog.open(); return
        case "comment": canvas.tool = "comment"; break
        case "saved": savedDialog.open(); return
        case "shortcuts": shortcutsDialog.open(); return
        }
        canvas.forceActiveFocus()
    }

    Card {
        id: toolbar
        x: 12
        // Centered, but never under the title bar (top) or the zoom bar (bottom).
        y: Math.max(72, (win.height - height) / 2)
        width: 52
        // Short windows (phones, small browser tabs): the tool column scrolls.
        height: Math.min(toolCol.implicitHeight + 12, win.height - 72 - 64)
        clip: true
        Flickable {
            anchors.fill: parent
            contentHeight: toolCol.implicitHeight + 12
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            ColumnLayout {
                id: toolCol
                x: 6
                y: 6
                spacing: 2
                ToolButtonIcon {
                    tip: qsTr("Select (V)")
                    iconPath: "M6 3 L6 19 L10.5 14.5 L13.5 21 L16 20 L13 13.5 L19 13.5 Z"
                    active: canvas.tool === "select"
                    onClicked: { canvas.tool = "select"; canvas.forceActiveFocus() }
                }
                Repeater {
                    model: AppState.pinnedTools
                    delegate: ToolButtonIcon {
                        id: tb
                        required property string modelData
                        readonly property var def: ToolRegistry.find(modelData)
                        visible: def !== null
                        tip: def ? def.name : ""
                        iconPath: def ? def.path : ""
                        active: win.toolActive(modelData)
                        onClicked: win.activateTool(modelData, tb)
                    }
                }
                Rectangle { Layout.alignment: Qt.AlignHCenter; implicitWidth: 28; implicitHeight: 1; color: Theme.border }
                ToolButtonIcon {
                    id: plusBtn
                    tip: qsTr("All tools — search and pin to the toolbar")
                    iconPath: "M12 5 V19 M5 12 H19"
                    active: toolsPanel.visible
                    onClicked: {
                        const p = plusBtn.mapToItem(win.contentItem, 0, 0)
                        toolsPanel.x = toolbar.x + toolbar.width + 8
                        toolsPanel.y = Math.max(8, Math.min(p.y - 200, win.height - toolsPanel.height - 16))
                        toolsPanel.open()
                    }
                }
            }
        }
    }

    // Tool options next to the toolbar (pen family, sticky colours, frame presets)
    ToolOptions {
        canvas: canvas
        x: toolbar.x + toolbar.width + 8
        y: Math.max(72, Math.min(toolbar.y, win.height - height - 64))
        z: 5
        onBulkRequested: bulkDialog.open()
    }

    ToolFlyout {
        id: flyout
        canvas: canvas
        onMoreShapesRequested: win.leftPanel = "shapes"
        onIconsRequested: win.leftPanel = "icons"
    }

    ToolsPanel {
        id: toolsPanel
        onToolActivated: (id) => win.activateTool(id, plusBtn)
    }

    ShapeLibraryPanel {
        canvas: canvas
        visible: win.leftPanel === "shapes"
        x: toolbar.x + toolbar.width + 8
        y: 72
        height: win.height - 72 - 64
        z: 6
        onClosed: win.leftPanel = ""
    }

    IconsPanel {
        canvas: canvas
        visible: win.leftPanel === "icons"
        x: toolbar.x + toolbar.width + 8
        y: 72
        height: win.height - 72 - 64
        z: 6
        onClosed: win.leftPanel = ""
    }

    LayersPanel {
        canvas: canvas
        visible: win.layersOpen
        x: win.width - width - 12
        y: 72
        height: Math.min(560, win.height - 72 - 64)
        z: 6
        onClosed: win.layersOpen = false
    }

    TemplatesDialog {
        id: templatesDialog
        canvas: canvas
        onChosen: (id) => {
            if (canvas.itemCount > 0 && canvas.modified) {
                pendingTemplate = id
                templateConfirm.open()
            } else {
                canvas.newFromTemplate(id)
            }
        }
        property string pendingTemplate: ""
    }

    Dialog {
        id: templateConfirm
        title: qsTr("Replace this board?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(420, win.width - 32)
        contentItem: Label {
            wrapMode: Text.WordWrap
            color: Theme.text
            text: qsTr("The template opens as a new board. Unsaved changes on the current board will be lost.")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { id: tplOk; highlighted: true; text: qsTr("Open template"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            onAccepted: canvas.newFromTemplate(templatesDialog.pendingTemplate)
        }
    }

    Dialog {
        id: bulkDialog
        title: qsTr("Bulk add sticky notes")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, win.width - 32)
        onOpened: { bulkText.text = ""; bulkText.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 8
            Label { text: qsTr("One idea per line — each line becomes a sticky note."); color: Theme.subText }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                TextArea { id: bulkText; placeholderText: qsTr("First idea\nSecond idea\nThird idea"); wrapMode: TextEdit.Wrap }
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { id: bulkOk; highlighted: true; text: qsTr("Add notes"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            onAccepted: canvas.addStickies(bulkText.text.split("\n"))
        }
    }

    // Selection context toolbar (shapes, notes, text, lines)
    // Floats above the selection with a gap (below it when there is no room);
    // hidden while the object is being dragged, resized or connected.
    PropertiesPanel {
        canvas: canvas
        visible: canvas.selectionCount > 0 && !canvas.editing && !canvas.interacting && canvas.selType !== "image"
                 && canvas.selType !== "mixed" && canvas.selType !== "comment" && canvas.selType !== "connector"
        readonly property bool phone: win.width < 700
        readonly property rect sel: canvas.selectionScreenRect
        transformOrigin: phone ? Item.TopLeft : Item.Top
        scale: Math.min(1, ((phone ? win.width - 76 : win.width) - 12) / implicitWidth)
        x: phone ? 70 : Math.max(8, Math.min(win.width - width - 8, sel.x + sel.width / 2 - width / 2))
        y: phone ? 68 : (sel.y - height - 40 >= 64 ? sel.y - height - 40
                                                   : Math.min(win.height - height - 64, sel.y + sel.height + 40))
        onMoreRequested: (anchor) => ctxMenu.popup(anchor, 0, anchor.height + 6)
    }

    ConnectorToolbar {
        canvas: canvas
        visible: canvas.selectionCount === 1 && canvas.selType === "connector" && !canvas.editing && !canvas.interacting
        scale: Math.min(1, (win.width - 16) / implicitWidth)
        onMoreRequested: (anchor) => ctxMenu.popup(anchor, 0, anchor.height + 6)
    }

    // Image toolbar, floating just above a selected image
    ImageToolbar {
        canvas: canvas
        visible: canvas.selectionCount === 1 && canvas.selType === "image" && !canvas.editing && !canvas.interacting
        scale: Math.min(1, (win.width - 16) / implicitWidth)
        onAltTextRequested: win.askAltText()
        onDownloadRequested: win.downloadOriginal()
        onMoreRequested: (anchor) => ctxMenu.popup(anchor, 0, anchor.height + 6)
    }

    BoardContextMenu {
        id: ctxMenu
        canvas: canvas
        onRenameRequested: textDialog.ask("rename", "Rename", "Name", canvas.selName)
        onAltTextRequested: win.askAltText()
        onDownloadRequested: win.downloadOriginal()
        onExportRequested: exportDialog.open()
        onLinkRequested: linkDialog.open()
        onTemplateRequested: textDialog.ask("template", qsTr("Save as template"), "", canvas.selName || canvas.boardTitle)
        onCursorChatRequested: (x, y) => cursorChat.start(x, y)
    }

    LinkDialog { id: linkDialog; canvas: canvas }

    Dialog {
        id: aboutDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(340, win.width - 32)
        padding: 24
        contentItem: ColumnLayout {
            spacing: 6
            MiruLogo { Layout.alignment: Qt.AlignHCenter; implicitWidth: 72; implicitHeight: 72 }
            Label { Layout.alignment: Qt.AlignHCenter; text: "Miru"; font.pixelSize: 22; font.weight: Font.Bold; color: Theme.text }
            Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Version %1").arg(Qt.application.version); color: Theme.subText }
            Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Free whiteboard for everyone"); color: Theme.subText }
            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: "© " + new Date().getFullYear() + " " + Qt.application.domain
                color: Theme.mutedText
                font.pixelSize: 12
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Close"); highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
    SavedFilesDialog { id: savedDialog; canvas: canvas }
    ShortcutsDialog { id: shortcutsDialog }
    CommentPopup { id: commentPopup; canvas: canvas }
    CursorChat { id: cursorChat; canvas: canvas }

    function askAltText() {
        textDialog.ask("alt", "Alt text",
                       "Describe the image for people using screen readers. It is saved in SVG exports.",
                       canvas.selAltText)
    }
    function downloadOriginal() {
        if (isWeb) {
            canvas.browserDownloadOriginal()
            return
        }
        originalDialog.selectedFile = originalDialog.currentFolder + "/" + canvas.originalImageName()
        originalDialog.open()
    }

    // Bottom-right: zoom
    Card {
        anchors { right: parent.right; bottom: parent.bottom; margins: 12 }
        height: 44
        width: zoomRow.implicitWidth + 12
        RowLayout {
            id: zoomRow
            anchors.centerIn: parent
            spacing: 0
            ToolButtonIcon { tip: "Zoom out (Ctrl+-)"; iconPath: "M5 12 H19"; onClicked: canvas.zoomOut() }
            ToolButton {
                id: zoomLabel
                text: Math.round(canvas.zoom * 100) + "%"
                focusPolicy: Qt.NoFocus
                implicitWidth: 64
                onClicked: canvas.resetZoom()
                ToolTip.visible: hovered
                ToolTip.text: "Reset to 100% (Ctrl+0)"
                background: Rectangle { radius: 6; color: zoomLabel.hovered ? Theme.hover : "transparent" }
                contentItem: Label { text: zoomLabel.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            ToolButtonIcon { tip: "Zoom in (Ctrl++)"; iconPath: "M12 5 V19 M5 12 H19"; onClicked: canvas.zoomIn() }
            ToolButtonIcon {
                tip: "Fit to screen (Ctrl+1)"
                iconPath: "M4 9 V4 H9 M15 4 H20 V9 M20 15 V20 H15 M9 20 H4 V15"
                onClicked: canvas.zoomToFit()
            }
        }
    }

    // Bottom-left: status
    Label {
        visible: win.width >= 700
        anchors { left: parent.left; bottom: parent.bottom; margins: 16 }
        color: Theme.mutedText
        text: (canvas.itemCount === 1 ? qsTr("1 object") : qsTr("%1 objects").arg(canvas.itemCount))
              + (canvas.selectionCount > 0 ? " · " + qsTr("%1 selected").arg(canvas.selectionCount) : "")
              + (canvas.boardPrivate ? " · Private board" : "")
    }

    // ------------------------------------------------------------------ toast
    Rectangle {
        id: toast
        property alias text: toastLabel.text
        function show(t) { text = t; opacity = 1; toastTimer.restart() }
        anchors { bottom: parent.bottom; bottomMargin: 72; horizontalCenter: parent.horizontalCenter }
        width: Math.min(toastLabel.implicitWidth + 32, win.width - 48)
        height: toastLabel.implicitHeight + 20
        radius: 10
        color: Theme.toast
        opacity: 0
        visible: opacity > 0
        z: 50
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Label {
            id: toastLabel
            anchors.centerIn: parent
            width: parent.width - 32
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: "white"
        }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }

    // ------------------------------------------------------------------ dialogs
    FileDialog {
        id: openDialog
        title: "Open board"
        nameFilters: ["Board files (*.board)", "All files (*)"]
        onAccepted: canvas.openFile(selectedFile)
    }
    FileDialog {
        id: saveDialog
        title: "Save board"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "board"
        nameFilters: ["Board files (*.board)"]
        onAccepted: { canvas.saveFile(selectedFile, win.pendingPassword); win.afterSave() }
        onRejected: { win.pendingPassword = ""; win.closeAfterSave = false }
    }
    FileDialog {
        id: imageDialog
        title: "Insert images"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.tif *.tiff *.svg)", "All files (*)"]
        onAccepted: canvas.addImagesFromUrls(selectedFiles, win.width / 2, win.height / 2)
    }
    FileDialog {
        id: originalDialog
        title: "Save original image"
        fileMode: FileDialog.SaveFile
        onAccepted: canvas.saveOriginalImage(selectedFile)
    }

    ExportDialog {
        id: exportDialog
        canvas: canvas
    }

    PasswordDialog {
        id: pwDialog
        onSubmitted: (password) => {
            switch (win.passwordPurpose) {
            case "makePrivate":
                if (canvas.makePrivate(password)) pwDialog.close()
                break
            case "makePublic":
                if (canvas.makePublic(password)) pwDialog.close()
                else pwDialog.showError("Wrong password.")
                break
            case "open":
                if (canvas.unlockPending(password)) pwDialog.close()
                else pwDialog.showError("Wrong password, or the file is damaged.")
                break
            case "save":
                if (!canvas.checkPassword(password)) {
                    pwDialog.showError("Wrong password.")
                    break
                }
                pwDialog.close()
                win.pendingPassword = password
                if (win.passwordAction) win.passwordAction()
                break
            }
        }
        onRejected: {
            if (win.passwordPurpose === "open")
                canvas.cancelPending()
            win.closeAfterSave = false
        }
    }

    // Rename / alt text
    Dialog {
        id: textDialog
        property string purpose: ""
        property string help: ""
        function ask(purpose, title, help, value) {
            textDialog.purpose = purpose
            textDialog.title = title
            textDialog.help = help
            textField.text = value
            open()
            textField.forceActiveFocus()
            textField.selectAll()
        }
        function apply() {
            if (purpose === "rename") canvas.renameSelection(textField.text)
            else if (purpose === "alt") canvas.selAltText = textField.text
            else if (purpose === "template") canvas.saveSelectionAsTemplate(textField.text)
            close()
        }
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(420, win.width - 32)
        padding: 20
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                visible: textDialog.help.length > 0 && textDialog.purpose === "alt"
                wrapMode: Text.WordWrap
                color: Theme.subText
                text: textDialog.help
            }
            TextField {
                id: textField
                Layout.fillWidth: true
                placeholderText: textDialog.purpose === "alt" ? "e.g. Team photo at the 2026 offsite"
                               : textDialog.purpose === "template" ? qsTr("Template name") : "Name"
                onAccepted: textDialog.apply()
            }
        }
        footer: DialogButtonBox {
            alignment: Qt.AlignRight
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { id: textGo; highlighted: true; text: qsTr("Save"); onClicked: textDialog.apply() }
        }
    }

    Dialog {
        id: infoDialog
        title: "Info"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(440, win.width - 32)
        padding: 20
        standardButtons: Dialog.Close
        contentItem: Label {
            text: infoDialog.visible ? canvas.selectionInfo() : ""
            wrapMode: Text.WordWrap
            color: Theme.text
            lineHeight: 1.3
        }
    }

    Dialog {
        id: unsavedDialog
        title: "Unsaved changes"
        modal: true
        width: Math.min(420, win.width - 32)
        anchors.centerIn: Overlay.overlay
        Label {
            width: parent ? parent.width : 380
            wrapMode: Text.WordWrap
            color: Theme.text
            text: "Save changes to \"" + canvas.boardTitle + "\" before closing?"
        }
        footer: DialogButtonBox {
            Button { id: unsavedSave; highlighted: true; text: qsTr("Save"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: "Don't save"; DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            Button { text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            onAccepted: {
                win.closeAfterSave = true
                win.save()
            }
            onDiscarded: { win.closeConfirmed = true; unsavedDialog.close(); win.close() }
        }
    }

    // ------------------------------------------------------------------ sign-in gate
    SignInPage {
        id: signIn
        anchors.fill: parent
        visible: false
        z: 100
        onDismissed: {
            visible = false
            win.afterSignIn = null
            win.closeAfterSave = false
            canvas.forceActiveFocus()
        }
    }

    // Share / live collaboration (starting a session needs sign-in; joining doesn't)
    LiveSessionDialog {
        id: shareDialog
        collab: collab
        cloud: cloud
        canvas: canvas
        onSaveCopyRequested: win.saveAs()
        onExportRequested: exportDialog.open()
    }

    // Start screen
    HomePage {
        anchors.fill: parent
        visible: win.showHome
        z: 90
        canvas: canvas
        onTemplateChosen: (id) => { canvas.newFromTemplate(id); win.showHome = false; canvas.forceActiveFocus() }
        onRecentChosen: (path) => win.openPath("file:///" + path.replace(/^\/+/, ""))
        onOpenRequested: { win.showHome = false; win.openBoard() }
        onJoinRequested: win.joinSession()
        onClosed: { win.showHome = false; canvas.forceActiveFocus() }
    }

    // ------------------------------------------------------------------ shortcuts
    Shortcut { sequences: [StandardKey.New]; onActivated: canvas.newBoard() }
    Shortcut { sequences: [StandardKey.Open]; onActivated: win.openBoard() }
    Shortcut { sequences: [StandardKey.Save]; onActivated: win.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: win.saveAs() }
    Shortcut { sequence: "Ctrl+E"; onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+I"; onActivated: win.insertImages() }
    Shortcut { sequence: "F1"; onActivated: shortcutsDialog.open() }
}
