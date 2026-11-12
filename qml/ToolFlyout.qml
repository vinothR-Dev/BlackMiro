import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Miru

// Pop-out menu next to the toolbar for "Shapes and lines" and "Stickers and emoji".
Popup {
    id: fly

    required property BoardCanvas canvas
    property string kind: "shapes"          // shapes | emoji
    readonly property string emojiFont: canvas.emojiFamily()
    signal moreShapesRequested()
    signal iconsRequested()

    padding: 8
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function pick(tool) {
        canvas.tool = tool
        close()
        canvas.forceActiveFocus()
    }

    component Row_: ItemDelegate {
        id: row
        property string glyph
        property string label
        property string shortcut
        implicitWidth: 236
        implicitHeight: 36
        padding: 6
        horizontalPadding: 10
        background: Rectangle { radius: 8; color: row.hovered ? Theme.hover : "transparent" }
        contentItem: RowLayout {
            spacing: 10
            Shape {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                preferredRendererType: Shape.CurveRenderer
                scale: 20 / 24
                transformOrigin: Item.TopLeft
                ShapePath {
                    strokeColor: Theme.text; strokeWidth: 1.8; fillColor: "transparent"
                    capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: row.glyph }
                }
            }
            Label { Layout.fillWidth: true; text: row.label; color: Theme.text }
            Label { text: row.shortcut; color: Theme.mutedText; font.pixelSize: 12; visible: text.length > 0 }
        }
    }

    contentItem: Loader {
        sourceComponent: fly.kind === "emoji" ? emojiContent : shapesContent
    }

    Component {
        id: shapesContent
        ColumnLayout {
            spacing: 0
            Row_ { glyph: "M5 19 L19 5"; label: qsTr("Line"); onClicked: fly.pick("line") }
            Row_ { glyph: "M5 19 L19 5 M12 5 H19 V12"; label: qsTr("Arrow"); shortcut: "L"; onClicked: fly.pick("arrow") }
            Row_ { glyph: "M4 19 H12 V5 H20 M16 2 L20 5 L16 8"; label: qsTr("Elbow arrow"); onClicked: fly.pick("elbow") }
            Row_ { glyph: "M2 9 H14 V5 L22 12 L14 19 V15 H2 Z"; label: qsTr("Block arrow"); onClicked: fly.pick("shape:arrow-right") }
            MenuSeparator { Layout.fillWidth: true }
            Row_ { glyph: "M4 6 H20 V18 H4 Z"; label: qsTr("Rectangle"); shortcut: "R"; onClicked: fly.pick("rectangle") }
            Row_ { glyph: "M8 6 H16 A4 4 0 0 1 20 10 V14 A4 4 0 0 1 16 18 H8 A4 4 0 0 1 4 14 V10 A4 4 0 0 1 8 6 Z"; label: qsTr("Rounded rectangle"); onClicked: fly.pick("rounded") }
            Row_ { glyph: "M4 12 A8 7 0 1 0 20 12 A8 7 0 1 0 4 12 Z"; label: qsTr("Oval"); shortcut: "O"; onClicked: fly.pick("ellipse") }
            Row_ { glyph: "M12 3 L21 12 L12 21 L3 12 Z"; label: qsTr("Rhombus"); onClicked: fly.pick("diamond") }
            Row_ { glyph: "M12 4 L21 20 H3 Z"; label: qsTr("Triangle"); onClicked: fly.pick("triangle") }
            Row_ { glyph: "M12 3 L14.6 8.9 L21 9.5 L16.2 13.7 L17.6 20 L12 16.8 L6.4 20 L7.8 13.7 L3 9.5 L9.4 8.9 Z"; label: qsTr("Star"); onClicked: fly.pick("shape:star") }
            Row_ { glyph: "M3 12 H21"; label: qsTr("Divider"); onClicked: fly.pick("divider") }
            MenuSeparator { Layout.fillWidth: true }
            Row_ {
                glyph: "M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M17 14 a3 3 0 1 0 0.01 0"
                label: qsTr("More shapes…")
                onClicked: { fly.close(); fly.moreShapesRequested() }
            }
        }
    }

    Component {
        id: emojiContent
        ColumnLayout {
            spacing: 6
            width: 300
            Label { text: qsTr("Stickers and emoji"); font.weight: Font.DemiBold; color: Theme.text; Layout.margins: 4 }
            // No system or bundled colour-emoji font: point to the icon library instead.
            ColumnLayout {
                visible: fly.emojiFont.length === 0
                Layout.fillWidth: true
                Layout.margins: 4
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 280
                    wrapMode: Text.WordWrap
                    color: Theme.subText
                    text: qsTr("No colour-emoji font is available here. Use the icon library instead.")
                }
                Button { text: qsTr("Open icons"); highlighted: true; onClicked: { fly.close(); fly.iconsRequested() } }
            }
            Repeater {
                model: fly.emojiFont.length === 0 ? [] : fly.canvas.emojiLibrary()
                delegate: ColumnLayout {
                    required property var modelData
                    spacing: 2
                    Label { text: modelData.category; color: Theme.mutedText; font.pixelSize: 12; Layout.leftMargin: 4 }
                    Grid {
                        columns: 8
                        spacing: 2
                        Repeater {
                            model: modelData.emoji
                            delegate: ToolButton {
                                required property string modelData
                                implicitWidth: 34; implicitHeight: 34
                                text: modelData
                                font.pixelSize: 20
                                font.family: fly.emojiFont
                                focusPolicy: Qt.NoFocus
                                onClicked: fly.pick("emoji:" + modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
