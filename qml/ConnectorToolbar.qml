import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import Miru

// Floating toolbar for a selected line or arrow: start and end caps, swap
// direction, line type (thickness, straight / elbow / curved, solid / dashed /
// dotted), colour, label, lock and more.
Rectangle {
    id: bar

    required property BoardCanvas canvas
    signal moreRequested(Item anchor)

    implicitWidth: row.implicitWidth + 12
    implicitHeight: 48
    radius: 12
    color: Theme.surface
    border.color: Theme.border

    readonly property var lineTypes: [
        { tip: qsTr("Straight"), path: "M4 18 L20 6" },
        { tip: qsTr("Elbow"), path: "M4 18 H12 V6 H20" },
        { tip: qsTr("Curved"), path: "M4 16 C9 4 15 20 20 8" }
    ]
    readonly property var dashTypes: [
        { tip: qsTr("Solid"), pattern: [] },
        { tip: qsTr("Dashed"), pattern: [3, 2] },
        { tip: qsTr("Dotted"), pattern: [0.6, 1.6] }
    ]

    component Sep: Rectangle { implicitWidth: 1; implicitHeight: 24; color: Theme.border }

    // Small popup listing "None" / "Arrow" for one end of the line.
    component CapButton: ToolButton {
        id: cap
        property bool arrow
        property bool atStart
        signal chosen(bool arrow)
        implicitHeight: 40
        implicitWidth: arrow ? 44 : 52
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.delay: 450
        ToolTip.text: atStart ? qsTr("Line start") : qsTr("Line end")
        background: Rectangle { radius: 8; color: cap.down ? Theme.pressed : cap.hovered ? Theme.hover : "transparent" }
        contentItem: Item {
            Label {
                anchors.centerIn: parent
                visible: !cap.arrow
                text: qsTr("None")
                color: Theme.text
                font.pixelSize: 13
            }
            Shape {
                anchors.centerIn: parent
                visible: cap.arrow
                width: 28; height: 24
                preferredRendererType: Shape.CurveRenderer
                transform: Scale { origin.x: 14; xScale: cap.atStart ? -1 : 1 }
                ShapePath {
                    strokeColor: Theme.text; strokeWidth: 1.8; fillColor: Theme.text
                    capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: "M3 12 H20 M19 7 L26 12 L19 17 Z" }
                }
            }
        }
        onClicked: capPop.open()
        Popup {
            id: capPop
            y: cap.height + 8
            padding: 6
            background: Rectangle { radius: 12; color: Theme.surface; border.color: Theme.border }
            Column {
                spacing: 2
                Repeater {
                    model: [false, true]
                    delegate: ItemDelegate {
                        required property bool modelData
                        width: 120; height: 36
                        highlighted: modelData === cap.arrow
                        text: modelData ? qsTr("Arrow") : qsTr("None")
                        onClicked: { cap.chosen(modelData); capPop.close() }
                    }
                }
            }
        }
    }

    // Toggle in the Type popup: draws a small path preview.
    component PathChoice: ToolButton {
        id: pc
        property string path
        property var dash: []
        property bool current
        implicitWidth: 44; implicitHeight: 36
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        background: Rectangle { radius: 8; color: pc.current ? Theme.accentSoft : pc.hovered ? Theme.hover : "transparent" }
        contentItem: Item {
            Shape {
                anchors.centerIn: parent
                width: 24; height: 24
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: pc.current ? Theme.accent : Theme.text
                    strokeWidth: 2
                    fillColor: "transparent"
                    capStyle: pc.dash.length ? ShapePath.FlatCap : ShapePath.RoundCap
                    strokeStyle: pc.dash.length ? ShapePath.DashLine : ShapePath.SolidLine
                    dashPattern: pc.dash.length ? pc.dash : [1, 0]
                    PathSvg { path: pc.path }
                }
            }
        }
    }

    // Sit above the line; drop below it when there is no room above.
    readonly property rect sel: canvas.selectionScreenRect
    readonly property real gap: 40
    x: Math.max(8, Math.min(parent.width - width - 8, sel.x + sel.width / 2 - width / 2))
    y: sel.y - height - gap >= 64 ? sel.y - height - gap : Math.min(parent.height - height - 64, sel.y + sel.height + gap)

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 2

        CapButton {
            Layout.leftMargin: 4
            atStart: true
            arrow: bar.canvas.selArrowStart
            enabled: !bar.canvas.selLocked
            onChosen: (a) => bar.canvas.selArrowStart = a
        }
        ToolButtonIcon {
            tip: qsTr("Swap direction")
            enabled: !bar.canvas.selLocked
            iconPath: "M7 4 L4 7 L7 10 M4 7 H17 a3 3 0 0 1 3 3 M17 20 L20 17 L17 14 M20 17 H7 a3 3 0 0 1 -3 -3"
            onClicked: bar.canvas.swapConnectorEnds()
        }
        CapButton {
            atStart: false
            arrow: bar.canvas.selArrowEnd
            enabled: !bar.canvas.selLocked
            onChosen: (a) => bar.canvas.selArrowEnd = a
        }
        Sep {}
        ToolButton {
            id: typeBtn
            implicitWidth: 52; implicitHeight: 44
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            enabled: !bar.canvas.selLocked
            ToolTip.visible: hovered
            ToolTip.delay: 450
            ToolTip.text: qsTr("Line type, thickness and pattern")
            background: Rectangle { radius: 8; color: typePop.opened ? Theme.accentSoft : typeBtn.hovered ? Theme.hover : "transparent" }
            contentItem: Column {
                spacing: 0
                Shape {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 24; height: 20
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: Theme.text; strokeWidth: 1.8; fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathSvg { path: bar.lineTypes[Math.max(0, bar.canvas.selLineType)].path }
                    }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Type")
                    font.pixelSize: 10
                    color: Theme.subText
                }
            }
            onClicked: typePop.open()
            Popup {
                id: typePop
                y: typeBtn.height + 10
                x: (typeBtn.width - width) / 2
                padding: 16
                background: Rectangle { radius: 14; color: Theme.surface; border.color: Theme.border }
                ColumnLayout {
                    spacing: 10
                    Slider {
                        id: thick
                        Layout.fillWidth: true
                        Layout.preferredWidth: 140
                        from: 1; to: 12; stepSize: 1; snapMode: Slider.SnapAlways
                        value: bar.canvas.selStrokeWidth
                        onMoved: bar.canvas.selStrokeWidth = value
                        ToolTip.visible: pressed || hovered
                        ToolTip.text: qsTr("Thickness: %1").arg(value)
                    }
                    RowLayout {
                        spacing: 4
                        Repeater {
                            model: bar.lineTypes
                            delegate: PathChoice {
                                required property var modelData
                                required property int index
                                path: modelData.path
                                current: bar.canvas.selLineType === index
                                ToolTip.visible: hovered
                                ToolTip.text: modelData.tip
                                onClicked: bar.canvas.selLineType = index
                            }
                        }
                    }
                    RowLayout {
                        spacing: 4
                        Repeater {
                            model: bar.dashTypes
                            delegate: PathChoice {
                                required property var modelData
                                required property int index
                                path: "M3 12 H21"
                                dash: modelData.pattern
                                current: bar.canvas.selDash === index
                                ToolTip.visible: hovered
                                ToolTip.text: modelData.tip
                                onClicked: bar.canvas.selDash = index
                            }
                        }
                    }
                }
            }
        }
        ToolButton {
            id: colorBtn
            implicitWidth: 40; implicitHeight: 40
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            enabled: !bar.canvas.selLocked
            ToolTip.visible: hovered
            ToolTip.delay: 450
            ToolTip.text: qsTr("Line colour")
            background: Rectangle { radius: 8; color: colorBtn.hovered ? Theme.hover : "transparent" }
            contentItem: Item {
                Rectangle {
                    anchors.centerIn: parent
                    width: 22; height: 22; radius: 11
                    color: bar.canvas.selStroke
                    border.color: Theme.border
                }
            }
            onClicked: colorPop.open()
            Popup {
                id: colorPop
                y: colorBtn.height + 8
                padding: 10
                background: Rectangle { radius: 12; color: Theme.surface; border.color: Theme.border }
                ColorSwatches {
                    colors: ["#1a1a1a", "#808080", "#e6e6e6", "#ffffff", "#4262ff", "#2d9bf0", "#12cdd4", "#0ca789",
                             "#8fd14f", "#fac710", "#f24726", "#da0063", "#9510ac", "#652cb3"]
                    current: bar.canvas.selStroke
                    onPicked: (c) => { bar.canvas.selStroke = c; colorPop.close() }
                }
            }
        }
        Sep {}
        ToolButtonIcon {
            tip: qsTr("Add text to the line")
            enabled: !bar.canvas.selLocked
            iconPath: "M3 5 H14 M8.5 5 V19 M15 13 H21 M18 10 V16"
            onClicked: bar.canvas.editSelectionText()
        }
        ToolButtonIcon {
            tip: bar.canvas.selLocked ? qsTr("Unlock (Ctrl+Shift+L)") : qsTr("Lock (Ctrl+Shift+L)")
            active: bar.canvas.selLocked
            iconPath: bar.canvas.selLocked ? "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 7 0 V11"
                                           : "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 6.8 -1.2"
            onClicked: bar.canvas.selLocked = !bar.canvas.selLocked
        }
        ToolButtonIcon {
            id: more
            tip: qsTr("More")
            iconPath: "M12 5.5 a1 1 0 1 0 0.01 0 M12 11.5 a1 1 0 1 0 0.01 0 M12 17.5 a1 1 0 1 0 0.01 0"
            onClicked: bar.moreRequested(more)
        }
    }
}
