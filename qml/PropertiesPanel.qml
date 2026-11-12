import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// Floating context toolbar for the current selection.
Rectangle {
    id: panel

    required property BoardCanvas canvas
    signal moreRequested(Item anchor)

    readonly property string type: canvas.selType
    readonly property bool isText: ["sticky", "text", "rectangle", "rounded", "ellipse", "diamond", "triangle", "frame", "shape"].indexOf(type) >= 0
    readonly property bool hasFill: ["sticky", "rectangle", "rounded", "ellipse", "diamond", "triangle", "frame", "shape"].indexOf(type) >= 0
    readonly property bool hasStroke: ["rectangle", "rounded", "ellipse", "diamond", "triangle", "pen", "connector", "shape", "icon"].indexOf(type) >= 0

    readonly property var stickyColors: ["#fff9b1", "#f5d128", "#d5f692", "#c9df56", "#93d275", "#67c6c0",
                                         "#a6ccf5", "#6cd8fa", "#ff9d48", "#f0939d", "#ea94bb", "#be88c7"]
    readonly property var paletteColors: ["transparent", "#ffffff", "#1a1a1a", "#808080", "#e6e6e6", "#4262ff",
                                          "#2d9bf0", "#12cdd4", "#0ca789", "#8fd14f", "#fac710", "#f24726",
                                          "#da0063", "#9510ac", "#652cb3", "#fef445", "#ffd8f4", "#cee741"]

    implicitWidth: row.implicitWidth + 16
    implicitHeight: 48
    radius: 12
    color: Theme.surface
    border.color: Theme.border

    component Sep: Rectangle { implicitWidth: 1; implicitHeight: 24; color: Theme.border }

    component ColorButton: ToolButton {
        id: cb
        property color swatch
        property string tip
        property var swatchColors: panel.paletteColors
        signal chosen(color c)
        implicitWidth: 36
        implicitHeight: 36
        focusPolicy: Qt.NoFocus
        ToolTip.visible: hovered
        ToolTip.text: tip
        ToolTip.delay: 450
        background: Rectangle { radius: 8; color: cb.hovered ? Theme.hover : "transparent" }
        contentItem: Item {
            Rectangle {
                anchors.centerIn: parent
                width: 20; height: 20; radius: 10
                color: Qt.color(cb.swatch).a === 0 ? "white" : cb.swatch
                border.color: Theme.border
            }
        }
        onClicked: pop.open()
        Popup {
            id: pop
            y: cb.height + 6
            padding: 10
            background: Rectangle { radius: 10; color: Theme.surface; border.color: Theme.border }
            ColorSwatches {
                colors: cb.swatchColors
                current: cb.swatch
                onPicked: (c) => { cb.chosen(c); pop.close() }
            }
        }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4

        ColorButton {
            visible: panel.hasFill
            tip: panel.type === "sticky" ? "Note colour" : "Fill"
            swatch: panel.canvas.selFill
            swatchColors: panel.type === "sticky" ? panel.stickyColors : panel.paletteColors
            onChosen: (c) => panel.canvas.selFill = c
        }
        ColorButton {
            visible: panel.hasStroke
            tip: "Border / line colour"
            swatch: panel.canvas.selStroke
            onChosen: (c) => panel.canvas.selStroke = c
        }
        SpinBox {
            visible: panel.hasStroke
            from: 0; to: 32
            value: panel.canvas.selStrokeWidth
            editable: true
            implicitWidth: 116
            focusPolicy: Qt.ClickFocus
            onValueModified: panel.canvas.selStrokeWidth = value
            ToolTip.visible: hovered
            ToolTip.text: "Line width"
        }

        Sep { visible: panel.isText }

        ColorButton {
            visible: panel.isText && panel.type !== "frame"
            tip: "Text colour"
            swatch: panel.canvas.selTextColor
            onChosen: (c) => panel.canvas.selTextColor = c
        }
        SpinBox {
            visible: panel.isText
            from: 6; to: 288
            value: panel.canvas.selFontSize
            editable: true
            implicitWidth: 120
            focusPolicy: Qt.ClickFocus
            onValueModified: panel.canvas.selFontSize = value
            ToolTip.visible: hovered
            ToolTip.text: "Font size"
        }
        ToolButtonIcon {
            visible: panel.isText
            tip: "Bold"
            active: panel.canvas.selBold
            iconPath: "M7 5 H13 a3.5 3.5 0 0 1 0 7 H7 Z M7 12 H14 a3.5 3.5 0 0 1 0 7 H7 Z"
            onClicked: panel.canvas.selBold = !panel.canvas.selBold
        }
        ToolButtonIcon {
            visible: panel.isText && panel.type !== "frame"
            tip: "Align left"
            active: panel.canvas.selTextAlign === 1
            iconPath: "M4 6 H20 M4 10 H14 M4 14 H20 M4 18 H14"
            onClicked: panel.canvas.selTextAlign = 1
        }
        ToolButtonIcon {
            visible: panel.isText && panel.type !== "frame"
            tip: "Align centre"
            active: panel.canvas.selTextAlign === 0
            iconPath: "M4 6 H20 M7 10 H17 M4 14 H20 M7 18 H17"
            onClicked: panel.canvas.selTextAlign = 0
        }
        ToolButtonIcon {
            visible: panel.isText && panel.type !== "frame"
            tip: "Align right"
            active: panel.canvas.selTextAlign === 2
            iconPath: "M4 6 H20 M10 10 H20 M4 14 H20 M10 18 H20"
            onClicked: panel.canvas.selTextAlign = 2
        }

        ToolButtonIcon {
            visible: panel.type === "connector"
            tip: panel.canvas.selElbow ? "Elbow line (click for straight)" : "Straight line (click for elbow)"
            active: panel.canvas.selElbow
            iconPath: panel.canvas.selElbow ? "M4 18 H12 V6 H20" : "M4 20 L20 4"
            onClicked: panel.canvas.selElbow = !panel.canvas.selElbow
        }

        Sep {}

        ToolButtonIcon {
            tip: "Bring to front"
            iconPath: "M8 8 H20 V20 H8 Z M4 16 V4 H16"
            onClicked: panel.canvas.bringToFront()
        }
        ToolButtonIcon {
            tip: "Send to back"
            iconPath: "M4 4 H16 V16 H4 Z M20 8 V20 H8"
            onClicked: panel.canvas.sendToBack()
        }
        ToolButtonIcon {
            tip: "Duplicate (Ctrl+D)"
            iconPath: "M8 8 H20 V20 H8 Z M16 8 V4 H4 V16 H8"
            onClicked: panel.canvas.duplicateSelection()
        }
        ToolButtonIcon {
            tip: "Delete (Del)"
            iconColor: Theme.danger
            iconPath: "M5 7 H19 M10 7 V4 H14 V7 M7 7 L8 20 H16 L17 7"
            onClicked: panel.canvas.deleteSelection()
        }
        Sep {}
        ToolButtonIcon {
            tip: panel.canvas.selLocked ? qsTr("Unlock (Ctrl+Shift+L)") : qsTr("Lock (Ctrl+Shift+L)")
            active: panel.canvas.selLocked
            iconPath: panel.canvas.selLocked ? "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 7 0 V11"
                                             : "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 6.8 -1.2"
            onClicked: panel.canvas.selLocked = !panel.canvas.selLocked
        }
        ToolButtonIcon {
            id: moreBtn
            tip: qsTr("More")
            iconPath: "M12 5.5 a1 1 0 1 0 0.01 0 M12 11.5 a1 1 0 1 0 0.01 0 M12 17.5 a1 1 0 1 0 0.01 0"
            onClicked: panel.moreRequested(moreBtn)
        }
    }
}
