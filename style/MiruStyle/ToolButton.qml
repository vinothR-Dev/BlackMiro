import QtQuick
import QtQuick.Templates as T

T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 6
    horizontalPadding: 10
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked || control.highlighted ? control.palette.highlight : control.palette.buttonText
        opacity: control.enabled ? 1 : 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 36
        implicitHeight: 36
        radius: 8
        color: control.down ? control.palette.dark
             : control.checked ? Qt.alpha(control.palette.highlight, 0.14)
             : control.hovered ? control.palette.midlight : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Qt.alpha(control.palette.highlight, 0.6)
    }
}
