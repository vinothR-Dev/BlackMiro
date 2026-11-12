import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

// Number field with − and + inside the rounded box.
T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             up.implicitIndicatorHeight, down.implicitIndicatorHeight)

    padding: 4
    leftPadding: 4 + (down.indicator ? down.indicator.width : 0)
    rightPadding: 4 + (up.indicator ? up.indicator.width : 0)
    hoverEnabled: true
    editable: true

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: control.palette.text
        selectionColor: Qt.alpha(control.palette.highlight, 0.35)
        selectedTextColor: control.palette.text
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        clip: width < implicitWidth
    }

    component Step: Rectangle {
        id: step
        property bool pressedState
        property bool hoveredState
        property string glyph
        implicitWidth: 30
        implicitHeight: 30
        radius: 6
        color: pressedState ? control.palette.dark : hoveredState ? control.palette.midlight : "transparent"
        Shape {
            anchors.centerIn: parent
            width: 12; height: 12
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: control.palette.buttonText
                strokeWidth: 1.7
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathSvg { path: step.glyph }
            }
        }
    }

    up.indicator: Step {
        x: control.mirrored ? 5 : control.width - width - 5
        y: (control.height - height) / 2
        glyph: "M2 6 H10 M6 2 V10"
        pressedState: control.up.pressed
        hoveredState: control.up.hovered
        opacity: control.value < control.to ? 1 : 0.35
    }

    down.indicator: Step {
        x: control.mirrored ? control.width - width - 5 : 5
        y: (control.height - height) / 2
        glyph: "M2 6 H10"
        pressedState: control.down.pressed
        hoveredState: control.down.hovered
        opacity: control.value > control.from ? 1 : 0.35
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 40
        radius: 8
        color: control.palette.base
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.activeFocus ? control.palette.highlight
                    : control.hovered ? control.palette.placeholderText : control.palette.mid
    }
}
