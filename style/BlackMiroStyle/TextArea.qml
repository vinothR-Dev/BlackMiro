import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.TextArea {
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding,
                            implicitBackgroundWidth + leftInset + rightInset,
                            placeholder.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding,
                             implicitBackgroundHeight + topInset + bottomInset,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: 10
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true

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
        implicitHeight: 80
        radius: 8
        color: control.palette.base
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.activeFocus ? control.palette.highlight
                    : control.hovered ? control.palette.placeholderText : control.palette.mid
    }
}
