import QtQuick
import QtQuick.Templates as T

// Secondary by default; `highlighted: true` = primary (accent); `flat: true` = text button.
T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 8
    horizontalPadding: 16
    spacing: 6
    hoverEnabled: true
    font.weight: Font.DemiBold

    readonly property color fill: highlighted
        ? (down ? Qt.darker(palette.highlight, 1.15) : hovered ? Qt.lighter(palette.highlight, 1.08) : palette.highlight)
        : flat ? (down ? palette.dark : hovered ? palette.midlight : "transparent")
               : (down ? palette.dark : hovered ? palette.midlight : palette.button)

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? control.palette.highlightedText
             : control.flat ? control.palette.highlight : control.palette.buttonText
        opacity: control.enabled ? 1 : 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 88
        implicitHeight: 38
        radius: 8
        color: control.fill
        opacity: control.enabled ? 1 : 0.55
        border.width: control.highlighted || control.flat ? 0 : 1
        border.color: control.palette.mid

        Behavior on color { ColorAnimation { duration: 120 } }

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
}
