import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// "More shapes": searchable, categorised shape library with colour presets.
// Pick a shape, then click or drag on the board to place it.
Rectangle {
    id: panel

    required property BoardCanvas canvas
    signal closed()

    width: 300
    radius: 14
    color: Theme.surface
    border.color: Theme.border

    readonly property var all: canvas.shapeLibrary()
    property string query: ""
    property var collapsed: ({})
    readonly property var fills: ["#ffffff", "#fff9b1", "#d5f692", "#a6ccf5", "#f0939d", "#be88c7", "#ffbd78", "#e6e6e6"]

    function matches(e) {
        if (query.length === 0) return true
        const q = query.toLowerCase()
        return e.name.toLowerCase().indexOf(q) >= 0 || e.keywords.toLowerCase().indexOf(q) >= 0
                || e.category.toLowerCase().indexOf(q) >= 0
    }
    function categories() {
        const out = []
        for (const e of all)
            if (out.indexOf(e.category) < 0) out.push(e.category)
        return out
    }
    function hex(c) { return String(c).replace("#", "") }

    Rectangle {   // soft shadow
        z: -1
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.margins: -1
        radius: 15
        color: Qt.alpha(Theme.dark ? "#000000" : "#1f2a44", Theme.dark ? 0.35 : 0.08)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Shapes"); font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
            ToolButtonIcon { implicitWidth: 32; implicitHeight: 32; tip: qsTr("Close"); iconPath: "M6 6 L18 18 M18 6 L6 18"; onClicked: panel.closed() }
        }
        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Search shapes")
            onTextChanged: panel.query = text
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Label { text: qsTr("Apply colors"); color: Theme.subText; font.pixelSize: 12 }
            Repeater {
                model: panel.fills
                delegate: Rectangle {
                    required property string modelData
                    width: 18; height: 18; radius: 9
                    color: modelData
                    border.width: Qt.colorEqual(panel.canvas.shapeFill, modelData) ? 2.5 : 1
                    border.color: Qt.colorEqual(panel.canvas.shapeFill, modelData) ? Theme.accent : Theme.border
                    TapHandler { onTapped: panel.canvas.shapeFill = parent.modelData }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Column {
                width: scroll.availableWidth
                spacing: 8
                Repeater {
                    model: panel.categories()
                    delegate: Column {
                        id: section
                        required property string modelData
                        readonly property var entries: panel.all.filter(e => e.category === modelData && panel.matches(e))
                        visible: entries.length > 0
                        width: parent.width
                        spacing: 4

                        ItemDelegate {
                            width: parent.width
                            implicitHeight: 30
                            padding: 4
                            background: Rectangle { radius: 6; color: parent.hovered ? Theme.hover : "transparent" }
                            contentItem: RowLayout {
                                Label { text: section.modelData; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
                                Label { text: section.entries.length; color: Theme.mutedText; font.pixelSize: 12 }
                                Label { text: panel.collapsed[section.modelData] ? "▸" : "▾"; color: Theme.mutedText }
                            }
                            onClicked: {
                                const c = Object.assign({}, panel.collapsed)
                                c[section.modelData] = !c[section.modelData]
                                panel.collapsed = c
                            }
                        }
                        Grid {
                            visible: !panel.collapsed[section.modelData] || panel.query.length > 0
                            columns: 6
                            spacing: 4
                            Repeater {
                                model: section.entries
                                delegate: ToolButton {
                                    required property var modelData
                                    implicitWidth: 42; implicitHeight: 42
                                    focusPolicy: Qt.NoFocus
                                    ToolTip.visible: hovered
                                    ToolTip.text: modelData.name
                                    readonly property bool current: panel.canvas.tool === "shape:" + modelData.id
                                    onClicked: { panel.canvas.tool = "shape:" + modelData.id; panel.canvas.forceActiveFocus() }
                                    background: Rectangle {
                                        radius: 8
                                        color: parent.current ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent"
                                        border.width: parent.current ? 1.5 : 0
                                        border.color: Theme.accent
                                    }
                                    contentItem: Image {
                                        source: "image://shapes/" + parent.modelData.id + "?fill=" + panel.hex(panel.canvas.shapeFill)
                                                + "&stroke=" + panel.hex(Theme.text)
                                        sourceSize: Qt.size(64, 64)
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.mutedText
            font.pixelSize: 12
            text: qsTr("Click a shape, then click or drag on the board. Your own SVG shapes can be dropped in as images.")
        }
    }
}
