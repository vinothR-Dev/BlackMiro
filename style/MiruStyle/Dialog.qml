import QtQuick
import QtQuick.Templates as T

T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    padding: 24
    topPadding: 8
    bottomPadding: 8

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110; easing.type: Easing.InCubic }
    }

    background: Item {
        Rectangle {   // soft shadow
            anchors.fill: parent
            anchors.topMargin: 10
            anchors.margins: -2
            radius: 18
            color: Qt.alpha(control.palette.shadow, 0.16)
        }
        Rectangle {
            anchors.fill: parent
            radius: 16
            color: control.palette.window
            border.color: control.palette.mid
        }
    }

    header: Text {
        text: control.title
        visible: control.title.length > 0
        elide: Text.ElideRight
        font.pixelSize: 18
        font.weight: Font.DemiBold
        color: control.palette.windowText
        leftPadding: 24
        rightPadding: 24
        topPadding: 22
        bottomPadding: 6
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: Rectangle {
        color: Qt.alpha("#0b0d12", 0.42)
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    T.Overlay.modeless: Rectangle {
        color: Qt.alpha("#0b0d12", 0.12)
    }
}
