import QtQuick
import QtQuick.Templates as T

T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 12

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.margins: -1
            radius: 14
            color: Qt.alpha(control.palette.shadow, 0.14)
        }
        Rectangle {
            anchors.fill: parent
            radius: 12
            color: control.palette.window
            border.color: control.palette.mid
        }
    }

    T.Overlay.modal: Rectangle { color: Qt.alpha("#0b0d12", 0.42) }
    T.Overlay.modeless: Rectangle { color: "transparent" }
}
