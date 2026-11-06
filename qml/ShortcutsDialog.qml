import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlackMiro

// Keyboard shortcuts (F1). Same keys as Miro where the feature exists.
Dialog {
    id: dlg

    title: qsTr("Keyboard shortcuts")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(760, parent ? parent.width - 32 : 760)
    height: Math.min(640, parent ? parent.height - 32 : 640)
    padding: 0
    standardButtons: Dialog.Close

    property string query: ""
    onOpened: { query = ""; search.text = ""; search.forceActiveFocus() }

    readonly property var groups: [
        { name: qsTr("Tools"), keys: [
            ["V", qsTr("Select")], ["H", qsTr("Hand (or hold Space)")], ["T", qsTr("Text")], ["N", qsTr("Sticky note")],
            ["S", qsTr("Shapes")], ["R", qsTr("Rectangle")], ["O", qsTr("Oval")], ["L", qsTr("Connection line")],
            ["P", qsTr("Pen")], ["E", qsTr("Eraser")], ["C", qsTr("Comment")], ["F", qsTr("Frame")],
            ["1 – 9", qsTr("Toolbar tools in order")], ["/", qsTr("Cursor chat")] ] },
        { name: qsTr("Edit"), keys: [
            ["Ctrl+Z", qsTr("Undo")], ["Ctrl+Shift+Z / Ctrl+Y", qsTr("Redo")], ["Ctrl+C", qsTr("Copy")],
            ["Ctrl+Shift+C", qsTr("Copy as image")], ["Ctrl+X", qsTr("Cut")], ["Ctrl+V", qsTr("Paste")],
            ["Ctrl+D", qsTr("Duplicate")], ["Alt+drag", qsTr("Drag a copy")], ["Alt+arrows", qsTr("Duplicate next to it")],
            ["Ctrl+Alt+C / V", qsTr("Copy / paste style")], ["Delete / Backspace", qsTr("Delete")],
            ["Ctrl+Backspace", qsTr("Clear text")], ["Enter", qsTr("Edit text")], ["Ctrl+B", qsTr("Bold")] ] },
        { name: qsTr("Select and arrange"), keys: [
            ["Ctrl+A", qsTr("Select all")], ["Shift+click", qsTr("Add to selection")], ["Esc", qsTr("Deselect")],
            ["Tab / Shift+Tab", qsTr("Next / previous object")], ["Ctrl+G", qsTr("Group")], ["Ctrl+Shift+G", qsTr("Ungroup")],
            ["Ctrl+Shift+L", qsTr("Lock / unlock")], ["Ctrl+Alt+K", qsTr("Link to")], ["PgUp", qsTr("Bring to front")],
            ["Shift+PgUp", qsTr("Bring forward")], ["PgDn", qsTr("Send to back")], ["Shift+PgDn", qsTr("Send backward")],
            ["Arrows", qsTr("Move 1 px (Shift: 10 px)")], ["Shift+drag", qsTr("Move along one axis")],
            ["Ctrl+drag", qsTr("Move without snapping")] ] },
        { name: qsTr("View"), keys: [
            ["Ctrl + / Ctrl −", qsTr("Zoom in / out")], ["Ctrl+0", qsTr("Zoom to 100%")], ["Alt+1", qsTr("Show all")],
            ["Alt+2", qsTr("Zoom to selection")], ["Ctrl+1", qsTr("Fit to screen")], ["G", qsTr("Show / hide grid")],
            ["Arrows", qsTr("Move around the board (nothing selected)")], ["Space+drag", qsTr("Pan")] ] },
        { name: qsTr("Board"), keys: [
            ["Ctrl+N", qsTr("New board")], ["Ctrl+O", qsTr("Open")], ["Ctrl+S", qsTr("Save")],
            ["Ctrl+Shift+S", qsTr("Save as")], ["Ctrl+E", qsTr("Export")], ["Ctrl+I", qsTr("Insert image")],
            ["F1", qsTr("This list")] ] }
    ]

    function matches(k) {
        const q = query.toLowerCase()
        return q.length === 0 || k[0].toLowerCase().indexOf(q) >= 0 || k[1].toLowerCase().indexOf(q) >= 0
    }

    contentItem: ColumnLayout {
        spacing: 0
        TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 16
            placeholderText: qsTr("Search shortcuts")
            onTextChanged: dlg.query = text
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            Flow {
                width: parent.width
                padding: 16
                topPadding: 0
                spacing: 24
                Repeater {
                    model: dlg.groups
                    delegate: ColumnLayout {
                        id: grp
                        required property var modelData
                        readonly property var shown: modelData.keys.filter(k => dlg.matches(k))
                        visible: shown.length > 0
                        width: Math.max(300, (dlg.availableWidth - 32 - 24) / 2)
                        spacing: 4
                        Label { text: grp.modelData.name; font.bold: true; color: Theme.text; Layout.bottomMargin: 4 }
                        Repeater {
                            model: grp.shown
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 12
                                Label { Layout.fillWidth: true; text: modelData[1]; color: Theme.subText; elide: Text.ElideRight }
                                Rectangle {
                                    implicitWidth: keyLabel.implicitWidth + 14
                                    implicitHeight: keyLabel.implicitHeight + 6
                                    radius: 6
                                    color: Theme.surfaceAlt
                                    border.color: Theme.border
                                    Label { id: keyLabel; anchors.centerIn: parent; text: modelData[0]; color: Theme.text; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
