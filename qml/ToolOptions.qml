import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Miru

// Options card shown next to the toolbar while a tool family is active:
// pen family (pen, highlighter, smart drawing, erasers, lasso), sticky notes, frames.
Rectangle {
    id: card

    required property BoardCanvas canvas
    signal bulkRequested()

    readonly property bool penFamily: ["pen", "highlighter", "smartpen", "eraser", "precision-eraser", "lasso"].indexOf(canvas.tool) >= 0
    readonly property string kind: penFamily ? "pen" : canvas.tool === "sticky" ? "sticky" : canvas.tool === "frame" ? "frame" : ""

    visible: kind.length > 0
    implicitWidth: loader.implicitWidth + 20
    implicitHeight: loader.implicitHeight + 20
    radius: 12
    color: Theme.surface
    border.color: Theme.border

    Rectangle {   // soft shadow
        z: -1
        anchors.fill: parent
        anchors.topMargin: 3
        anchors.margins: -1
        radius: 13
        color: Qt.alpha(Theme.dark ? "#000000" : "#1f2a44", Theme.dark ? 0.35 : 0.07)
    }

    Loader {
        id: loader
        anchors.centerIn: parent
        sourceComponent: card.kind === "pen" ? penOptions : card.kind === "sticky" ? stickyOptions : card.kind === "frame" ? frameOptions : null
    }

    // ---------------------------------------------------------------- pen family
    Component {
        id: penOptions
        ColumnLayout {
            spacing: 6
            Repeater {
                model: [
                    { tool: "pen", tip: qsTr("Pen (P)"), path: "M4 20 L8 19 L19 8 L16 5 L5 16 Z M14 7 L17 10" },
                    { tool: "highlighter", tip: qsTr("Highlighter"), path: "M9 17 L4 22 H10 L12 20 M8 15 L15 4 L20 9 L12 19 Z" },
                    { tool: "smartpen", tip: qsTr("Smart drawing — sketch a box, circle, triangle or line and it becomes a clean shape"),
                      path: "M3 13 H10 V20 H3 Z M14 4 C17 3 21 6 19 10 C17 14 12 11 14 8" },
                    { tool: "eraser", tip: qsTr("Eraser — removes whole strokes"), path: "M7 20 H20 M4 15 L13 6 L19 12 L11 20 H8 Z M9 11 L14 16" },
                    { tool: "precision-eraser", tip: qsTr("Precision eraser — erases just the part you touch"), path: "M7 20 H20 M4 15 L13 6 L19 12 L11 20 H8 Z M15 18 a2 2 0 1 0 0.01 0" },
                    { tool: "lasso", tip: qsTr("Lasso — draw around things to select them"), path: "M12 4 C6 4 3 7 3 10 C3 13 7 15 12 15 C17 15 21 13 21 10 C21 7 18 4 12 4 Z M7 14 C6 17 8 20 10 20" }
                ]
                delegate: ToolButtonIcon {
                    required property var modelData
                    tip: modelData.tip
                    iconPath: modelData.path
                    active: card.canvas.tool === modelData.tool
                    onClicked: { card.canvas.tool = modelData.tool; card.canvas.forceActiveFocus() }
                }
            }
            Rectangle { Layout.alignment: Qt.AlignHCenter; implicitWidth: 28; implicitHeight: 1; color: Theme.border }
            // Presets: colour + width
            Repeater {
                model: [ { c: "#1a1a1a", w: 3 }, { c: "#f24726", w: 6 }, { c: "#2d9bf0", w: 10 } ]
                delegate: ToolButton {
                    required property var modelData
                    implicitWidth: 40; implicitHeight: 34
                    focusPolicy: Qt.NoFocus
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Pen preset")
                    readonly property bool current: Qt.colorEqual(card.canvas.penColor, modelData.c) && card.canvas.penWidth === modelData.w
                    onClicked: { card.canvas.penColor = modelData.c; card.canvas.penWidth = modelData.w; if (card.canvas.tool !== "pen" && card.canvas.tool !== "smartpen") card.canvas.tool = "pen" }
                    background: Rectangle {
                        radius: 8
                        color: parent.current ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent"
                    }
                    contentItem: Item {
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8 + parent.parent.modelData.w; height: width; radius: width / 2
                            color: parent.parent.modelData.c
                        }
                    }
                }
            }
            ColorSwatches {
                Layout.alignment: Qt.AlignHCenter
                columns: 2
                colors: ["#1a1a1a", "#f24726", "#fac710", "#8fd14f", "#2d9bf0", "#9510ac"]
                current: card.canvas.penColor
                onPicked: (c) => card.canvas.penColor = c
            }
            Slider {
                Layout.preferredWidth: 64
                Layout.alignment: Qt.AlignHCenter
                from: 1; to: 24; stepSize: 1
                value: card.canvas.penWidth
                onMoved: card.canvas.penWidth = value
                ToolTip.visible: pressed
                ToolTip.text: Math.round(value) + " px"
            }
        }
    }

    // ---------------------------------------------------------------- sticky notes
    Component {
        id: stickyOptions
        ColumnLayout {
            spacing: 10
            ColorSwatches {
                columns: 4
                colors: ["#fff9b1", "#f5d128", "#ffbd78", "#ff9d48", "#ffd8f4", "#f0939d", "#ea94bb", "#be88c7",
                         "#a6ccf5", "#6cd8fa", "#67c6c0", "#93d275", "#d5f692", "#c9df56", "#f1f1f1", "#2b2b2b"]
                current: card.canvas.stickyColor
                onPicked: (c) => card.canvas.stickyColor = c
            }
            Button {
                Layout.fillWidth: true
                checkable: true
                checked: card.canvas.stickyStack
                highlighted: checked
                text: qsTr("Stack")
                onToggled: card.canvas.stickyStack = checked
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Keep the sticky tool on and drop one note after another")
            }
            Button {
                Layout.fillWidth: true
                flat: true
                text: qsTr("Bulk add…")
                onClicked: card.bulkRequested()
            }
        }
    }

    // ---------------------------------------------------------------- frame presets
    Component {
        id: frameOptions
        Grid {
            columns: 3
            spacing: 4
            Repeater {
                model: [
                    { n: qsTr("Custom"), w: 960, h: 540, r: 1.6 },
                    { n: "A4", w: 794, h: 1123, r: 0.707 },
                    { n: qsTr("Letter"), w: 816, h: 1056, r: 0.773 },
                    { n: "16:9", w: 1280, h: 720, r: 1.78 },
                    { n: "4:3", w: 1024, h: 768, r: 1.33 },
                    { n: "1:1", w: 800, h: 800, r: 1 },
                    { n: qsTr("Mobile"), w: 390, h: 844, r: 0.46 },
                    { n: qsTr("Tablet"), w: 820, h: 1180, r: 0.69 },
                    { n: qsTr("Desktop"), w: 1440, h: 1024, r: 1.4 }
                ]
                delegate: ToolButton {
                    required property var modelData
                    implicitWidth: 64; implicitHeight: 62
                    focusPolicy: Qt.NoFocus
                    readonly property bool current: card.canvas.framePreset.width === modelData.w && card.canvas.framePreset.height === modelData.h
                    onClicked: card.canvas.framePreset = Qt.size(modelData.w, modelData.h)
                    ToolTip.visible: hovered
                    ToolTip.text: modelData.w + " × " + modelData.h
                    background: Rectangle { radius: 8; color: parent.current ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent" }
                    contentItem: ColumnLayout {
                        spacing: 4
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            readonly property real s: 26
                            implicitWidth: parent.parent.modelData.r >= 1 ? s : s * parent.parent.modelData.r
                            implicitHeight: parent.parent.modelData.r >= 1 ? s / parent.parent.modelData.r : s
                            radius: 3
                            color: "transparent"
                            border.width: 1.6
                            border.color: parent.parent.current ? Theme.accent : Theme.text
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: parent.parent.modelData.n
                            font.pixelSize: 11
                            color: Theme.subText
                        }
                    }
                }
            }
        }
    }
}
