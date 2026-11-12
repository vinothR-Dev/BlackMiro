import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// Right-click (or press-and-hold) menus, laid out like Miro's:
// one for the empty board, one for the selected object(s).
Item {
    id: root

    required property BoardCanvas canvas
    signal renameRequested()
    signal exportRequested()
    signal altTextRequested()
    signal downloadRequested()
    signal linkRequested()
    signal templateRequested()
    signal cursorChatRequested(real x, real y)

    readonly property bool hasSel: canvas.selectionCount > 0
    readonly property bool single: canvas.selectionCount === 1
    readonly property bool isImage: canvas.selType === "image"
    property var frameList: []
    property var meta: ({})
    property point at: Qt.point(0, 0)

    function popup(parentItem, x, y) {
        at = parentItem.mapToItem(canvas, x, y)
        if (hasSel) {
            meta = canvas.selectionMeta()
            itemMenu.popup(parentItem, x, y)
        } else {
            frameList = canvas.frames()
            boardMenu.popup(parentItem, x, y)
        }
    }

    function openInfo() { meta = canvas.selectionMeta(); infoMenu.popup(canvas, 990, 560) }   // screenshot hook

    // Menu entry with a right-aligned shortcut hint (the style renders "label\tshortcut").
    component Item_: MenuItem {
        property string label
        property string shortcut
        text: shortcut.length > 0 ? label + "\t" + shortcut : label
        height: visible ? implicitHeight : 0
    }
    component Sep_: MenuSeparator {
        height: visible ? implicitHeight : 0
    }

    // ------------------------------------------------------------ empty board
    Menu {
        id: boardMenu
        width: 290

        Item_ { label: qsTr("Paste"); shortcut: "Ctrl+V"; onTriggered: root.canvas.paste() }
        Item_ { label: qsTr("Select all"); shortcut: "Ctrl+A"; onTriggered: root.canvas.selectAll() }
        Item_ { label: qsTr("Unlock all"); onTriggered: root.canvas.unlockAll() }
        MenuSeparator {}
        Item_ { label: qsTr("Add text"); shortcut: "T"; onTriggered: root.canvas.addTextAt(root.at.x, root.at.y) }
        Item_ { label: qsTr("Add sticky note"); shortcut: "N"; onTriggered: root.canvas.addStickyAt(root.at.x, root.at.y) }
        Item_ { label: qsTr("Add comment"); shortcut: "C"; onTriggered: root.canvas.addCommentAt(root.at.x, root.at.y) }
        MenuSeparator {}
        Item_ { label: qsTr("Start cursor chat"); shortcut: "/"; onTriggered: root.cursorChatRequested(root.at.x, root.at.y) }
        MenuSeparator {}
        Menu {
            id: startMenu
            title: qsTr("Set start view")
            Instantiator {
                model: root.frameList
                delegate: MenuItem {
                    required property var modelData
                    text: qsTr("Frame: %1").arg(modelData.name)
                    onTriggered: root.canvas.setStartViewToFrame(modelData.id)
                }
                onObjectAdded: (index, object) => startMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => startMenu.removeItem(object)
            }
            MenuItem { enabled: false; text: qsTr("No frames on this board"); visible: root.frameList.length === 0; height: visible ? implicitHeight : 0 }
            MenuSeparator {}
            MenuItem { text: qsTr("Go to start view"); enabled: root.canvas.hasStartView; onTriggered: root.canvas.goToStartView() }
            MenuItem { text: qsTr("Remove start view"); enabled: root.canvas.hasStartView; onTriggered: root.canvas.clearStartView() }
        }
        Item_ { label: qsTr("Set current view as start"); onTriggered: root.canvas.setStartViewToCurrent() }
        MenuSeparator {}
        Item_ { label: qsTr("Show grid"); shortcut: "G"; checkable: true; checked: root.canvas.showGrid; onTriggered: root.canvas.showGrid = checked }
        Menu {
            title: qsTr("Grid type")
            MenuItem { text: qsTr("Dots"); checkable: true; checked: root.canvas.gridStyle === "dots"; onTriggered: { root.canvas.gridStyle = "dots"; root.canvas.showGrid = true } }
            MenuItem { text: qsTr("Lines"); checkable: true; checked: root.canvas.gridStyle === "lines"; onTriggered: { root.canvas.gridStyle = "lines"; root.canvas.showGrid = true } }
        }
        Item_ { label: qsTr("Show comments"); checkable: true; checked: root.canvas.showComments; onTriggered: root.canvas.showComments = checked }
        MenuSeparator {}
        Item_ { label: qsTr("Snap to grid"); checkable: true; checked: root.canvas.snapToGrid; onTriggered: root.canvas.snapToGrid = checked }
        Item_ { label: qsTr("Snap objects"); checkable: true; checked: root.canvas.snapObjects; onTriggered: root.canvas.snapObjects = checked }
        Item_ { label: qsTr("Show object size"); checkable: true; checked: root.canvas.showObjectSize; onTriggered: root.canvas.showObjectSize = checked }
        Item_ { label: qsTr("Suggest object size"); checkable: true; checked: root.canvas.suggestSize; onTriggered: root.canvas.suggestSize = checked }
        MenuSeparator {}
        Menu {
            title: qsTr("Mouse or trackpad")
            MenuItem { text: qsTr("Auto-switch"); checkable: true; checked: root.canvas.navMode === "auto"; onTriggered: root.canvas.navMode = "auto" }
            MenuItem { text: qsTr("Mouse"); checkable: true; checked: root.canvas.navMode === "mouse"; onTriggered: root.canvas.navMode = "mouse" }
            MenuItem { text: qsTr("Trackpad"); checkable: true; checked: root.canvas.navMode === "trackpad"; onTriggered: root.canvas.navMode = "trackpad" }
        }
        Item_ { label: qsTr("Show all"); shortcut: "Alt+1"; onTriggered: root.canvas.zoomToContent() }
    }

    // ------------------------------------------------------------ object(s)
    Menu {
        id: itemMenu
        width: 290

        Item_ { label: qsTr("Copy"); shortcut: "Ctrl+C"; onTriggered: root.canvas.copySelection() }
        Item_ { label: qsTr("Copy link"); onTriggered: root.canvas.copyLink() }
        Item_ { label: qsTr("Copy as image"); shortcut: "Ctrl+Shift+C"; onTriggered: root.canvas.copyAsPng() }
        Item_ { label: qsTr("Duplicate"); shortcut: "Ctrl+D"; onTriggered: root.canvas.duplicateSelection() }
        Item_ { label: qsTr("Delete"); shortcut: "Delete"; enabled: !root.canvas.selLocked; onTriggered: root.canvas.deleteSelection() }
        MenuSeparator {}
        Item_ { label: qsTr("Add comment"); shortcut: "C"; visible: root.canvas.selType !== "comment"; onTriggered: root.canvas.addCommentAt(-1, -1) }
        Sep_ { visible: root.canvas.selType !== "comment" }
        Item_ { label: qsTr("Scale to original size"); visible: root.single && root.isImage; onTriggered: root.canvas.resetImageSize() }
        Item_ { label: qsTr("Add to Saved files"); onTriggered: root.canvas.addSelectionToSaved() }
        Item_ { label: qsTr("Download"); visible: root.single && root.isImage; onTriggered: root.downloadRequested() }
        MenuSeparator {}
        Menu {
            id: arrangeMenu
            title: qsTr("Arrange")
            Item_ { label: qsTr("Bring forward"); shortcut: "Shift+PgUp"; onTriggered: root.canvas.bringForward() }
            Item_ { label: qsTr("Bring to front"); shortcut: "PgUp"; onTriggered: root.canvas.bringToFront() }
            Item_ { label: qsTr("Send backward"); shortcut: "Shift+PgDn"; onTriggered: root.canvas.sendBackward() }
            Item_ { label: qsTr("Send to back"); shortcut: "PgDn"; onTriggered: root.canvas.sendToBack() }
            MenuSeparator {}
            Item_ { label: qsTr("Line up in a row"); shortcut: "\\"; enabled: root.canvas.selectionCount > 1; onTriggered: root.canvas.arrangeSelection(false) }
            Item_ { label: qsTr("Line up in a column"); shortcut: "Shift+\\"; enabled: root.canvas.selectionCount > 1; onTriggered: root.canvas.arrangeSelection(true) }
            MenuSeparator {}
            Menu {
                id: layerMenu
                title: qsTr("Move to layer")
                MenuItem {
                    text: qsTr("Default layer")
                    checkable: true
                    checked: root.canvas.selLayer === ""
                    onTriggered: root.canvas.moveSelectionToLayer("")
                }
                Instantiator {
                    model: root.canvas.layers
                    delegate: MenuItem {
                        required property var modelData
                        visible: modelData.id !== ""
                        height: visible ? implicitHeight : 0
                        text: modelData.name
                        checkable: true
                        checked: root.canvas.selLayer === modelData.id
                        onTriggered: root.canvas.moveSelectionToLayer(modelData.id)
                    }
                    onObjectAdded: (index, object) => layerMenu.insertItem(index + 1, object)
                    onObjectRemoved: (index, object) => layerMenu.removeItem(object)
                }
            }
        }
        Item_ {
            visible: root.canvas.selectionCount > 1 || root.canvas.selGrouped
            label: root.canvas.selGrouped ? qsTr("Ungroup") : qsTr("Group")
            shortcut: root.canvas.selGrouped ? "Ctrl+Shift+G" : "Ctrl+G"
            onTriggered: root.canvas.selGrouped ? root.canvas.ungroupSelection() : root.canvas.groupSelection()
        }
        Item_ { label: root.canvas.selLink.length > 0 ? qsTr("Edit link") : qsTr("Link to"); shortcut: "Ctrl+Alt+K"; onTriggered: root.linkRequested() }
        Item_ {
            label: root.canvas.selLocked ? qsTr("Unlock") : qsTr("Lock")
            shortcut: "Ctrl+Shift+L"
            onTriggered: root.canvas.selLocked = !root.canvas.selLocked
        }
        MenuSeparator {}
        Item_ { label: qsTr("Create frame"); onTriggered: root.canvas.createFrameAroundSelection() }
        Item_ { label: qsTr("Save as template"); onTriggered: root.templateRequested() }
        MenuSeparator {}
        Menu {
            title: qsTr("Image")
            enabled: root.single && root.isImage
            Item_ { label: qsTr("Flip horizontally"); onTriggered: root.canvas.flipSelection(true) }
            Item_ { label: qsTr("Flip vertically"); onTriggered: root.canvas.flipSelection(false) }
            Item_ { label: qsTr("Crop"); onTriggered: root.canvas.startCrop() }
            Item_ { label: qsTr("Reset crop"); enabled: root.canvas.selCropped; onTriggered: root.canvas.resetCrop() }
            Item_ { label: qsTr("Alt text…"); onTriggered: root.altTextRequested() }
        }
        Menu {
            title: qsTr("More")
            Item_ { label: qsTr("Rename"); enabled: root.single; onTriggered: root.renameRequested() }
            Item_ { label: qsTr("Copy as SVG"); onTriggered: root.canvas.copyAsSvg() }
            Item_ { label: qsTr("Copy style"); shortcut: "Ctrl+Alt+C"; onTriggered: root.canvas.copyStyle() }
            Item_ { label: qsTr("Paste style"); shortcut: "Ctrl+Alt+V"; enabled: root.canvas.canPasteStyle; onTriggered: root.canvas.pasteStyle() }
            Item_ { label: qsTr("Zoom to object"); shortcut: "Alt+2"; onTriggered: root.canvas.zoomToSelection() }
            Item_ { label: qsTr("Export selection…"); onTriggered: root.exportRequested() }
        }
        Menu {
            id: infoMenu
            title: qsTr("Info")
            width: 300

            ColumnLayout {
                width: infoMenu.availableWidth
                spacing: 10

                component InfoRow: ColumnLayout {
                    property string caption
                    property string value
                    property string detail
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 2
                    Label { text: parent.caption; color: Theme.mutedText; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: parent.value
                        color: Theme.text
                        elide: Text.ElideMiddle
                        ToolTip.visible: parent.detail.length > 0 && infoHover.hovered
                        ToolTip.text: parent.detail
                        HoverHandler { id: infoHover }
                    }
                }

                Item { Layout.preferredHeight: 2 }
                InfoRow { caption: qsTr("Board owner"); value: root.meta.owner || "" }
                InfoRow {
                    caption: qsTr("Last modified")
                    value: (root.meta.modifiedBy || "") + " · " + (root.meta.modifiedAt || "")
                    detail: root.meta.modifiedExact || ""
                }
                InfoRow {
                    caption: qsTr("Created")
                    value: (root.meta.createdBy || "") + " · " + (root.meta.createdAt || "")
                    detail: root.meta.createdExact || ""
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 6
                    Layout.bottomMargin: 6
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: root.isImage ? qsTr("File name") : qsTr("Name"); color: Theme.mutedText; font.pixelSize: 12 }
                        Label { Layout.fillWidth: true; text: root.meta.fileName || ""; color: Theme.text; elide: Text.ElideMiddle }
                    }
                    ToolButton {
                        text: qsTr("Copy")
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            clip.text = root.meta.fileName || ""
                            clip.selectAll()
                            clip.copy()
                            text = qsTr("Copied")
                        }
                        onVisibleChanged: text = qsTr("Copy")
                        TextEdit { id: clip; visible: false }
                    }
                }
            }
        }
    }
}
