import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: 9
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true
    verticalAlignment: TextInput.AlignVCenter

    color: palette.text
    selectionColor: Qt.alpha(palette.highlight, 0.35)
    selectedTextColor: palette.text
    placeholderTextColor: palette.placeholderText

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        elide: Text.ElideRight
        renderType: control.renderType
        visible: !control.length && !control.preeditText
                 && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
    }

    background: Rectangle {
        implicitWidth: 220
        implicitHeight: 40
        radius: 8
        color: control.enabled ? control.palette.base : control.palette.alternateBase
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.activeFocus ? control.palette.highlight
                    : control.hovered ? control.palette.placeholderText : control.palette.mid

        Behavior on border.color { ColorAnimation { duration: 120 } }

        Rectangle {   // soft focus halo
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: 3
            border.color: Qt.alpha(control.palette.highlight, 0.18)
            visible: control.activeFocus
        }
    }
}
