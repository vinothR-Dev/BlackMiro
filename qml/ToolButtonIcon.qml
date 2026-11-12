import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Miru

ToolButton {
    id: root

    property string iconPath
    property string tip
    property bool active: false
    property color iconColor: !enabled ? Theme.iconDisabled : active ? Theme.accent : Theme.text

    implicitWidth: 40
    implicitHeight: 40
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 450

    background: Rectangle {
        radius: 8
        color: root.active ? Theme.accentSoft : root.down ? Theme.pressed : root.hovered ? Theme.hover : "transparent"
    }

    contentItem: Item {
        Shape {
            anchors.centerIn: parent
            width: 24
            height: 24
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: root.iconColor
                strokeWidth: 1.8
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: root.iconPath }
            }
        }
    }
}
