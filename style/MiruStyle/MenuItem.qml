import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 7
    horizontalPadding: 12
    spacing: 10
    hoverEnabled: true

    indicator: Shape {
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 16; height: 16
        visible: control.checked
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: control.palette.highlight
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M3 8.5 L6.5 12 L13 4.5" }
        }
    }

    arrow: Shape {
        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 14; height: 14
        visible: control.subMenu
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: control.palette.placeholderText
            strokeWidth: 1.6
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M5 3 L10 7 L5 11" }
        }
    }

    contentItem: Text {
        readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
        readonly property real indicatorPadding: control.checkable && control.indicator ? control.indicator.width + control.spacing : 0
        leftPadding: !control.mirrored ? indicatorPadding : arrowPadding
        rightPadding: control.mirrored ? indicatorPadding : arrowPadding

        text: control.text.split("\t")[0]
        font: control.font
        color: control.palette.windowText
        opacity: control.enabled ? 1 : 0.4
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter

        Text {   // shortcut hint after a tab: "Save\tCtrl+S"
            anchors.right: parent.right
            anchors.rightMargin: parent.rightPadding
            anchors.verticalCenter: parent.verticalCenter
            text: control.text.indexOf("\t") >= 0 ? control.text.split("\t")[1] : ""
            font.pixelSize: control.font.pixelSize - 2
            color: control.palette.placeholderText
            opacity: control.enabled ? 1 : 0.4
        }
    }

    background: Rectangle {
        implicitWidth: 220
        implicitHeight: 34
        radius: 7
        color: control.down ? control.palette.dark
             : control.highlighted ? control.palette.midlight : "transparent"
    }
}
