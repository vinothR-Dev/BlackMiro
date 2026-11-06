import QtQuick
import QtQuick.Templates as T

T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 8
    overlap: 4
    padding: 6

    delegate: MenuItem { }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80 }
    }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex
    }

    background: Item {
        implicitWidth: 240
        implicitHeight: 40
        Rectangle {   // soft shadow
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.margins: -1
            radius: 14
            color: Qt.alpha(control.palette.shadow, 0.16)
        }
        Rectangle {
            anchors.fill: parent
            radius: 12
            color: control.palette.window
            border.color: control.palette.mid
        }
    }

    T.Overlay.modal: Rectangle { color: Qt.alpha("#0b0d12", 0.3) }
    T.Overlay.modeless: Rectangle { color: "transparent" }
}
