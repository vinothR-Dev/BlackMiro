import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 4
    spacing: 10
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 6
        color: control.checkState !== Qt.Unchecked ? control.palette.highlight : control.palette.base
        border.width: control.checkState !== Qt.Unchecked ? 0 : 1.5
        border.color: control.hovered ? control.palette.highlight : control.palette.placeholderText
        opacity: control.enabled ? 1 : 0.45

        Behavior on color { ColorAnimation { duration: 120 } }

        Shape {
            anchors.centerIn: parent
            width: 14; height: 14
            visible: control.checkState === Qt.Checked
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: control.palette.highlightedText
                strokeWidth: 2.2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M2.5 7.5 L5.8 10.6 L11.5 3.8" }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 10; height: 2; radius: 1
            visible: control.checkState === Qt.PartiallyChecked
            color: control.palette.highlightedText
        }
        Rectangle {   // keyboard focus ring
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Qt.alpha(control.palette.highlight, 0.5)
            visible: control.visualFocus
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        font: control.font
        color: control.palette.windowText
        opacity: control.enabled ? 1 : 0.45
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
}
