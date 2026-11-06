import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlackMiro

// Floating toolbar shown just above a selected image.
Rectangle {
    id: bar

    required property BoardCanvas canvas
    signal altTextRequested()
    signal downloadRequested()
    signal moreRequested(Item anchor)

    implicitWidth: row.implicitWidth + 12
    implicitHeight: 48
    radius: 12
    color: Theme.surface
    border.color: Theme.border

    // Sit above the image; drop below it when there is no room above.
    readonly property rect sel: canvas.selectionScreenRect
    readonly property real gap: 40
    x: Math.max(8, Math.min(parent.width - width - 8, sel.x + sel.width / 2 - width / 2))
    y: sel.y - height - gap >= 64 ? sel.y - height - gap : Math.min(parent.height - height - 64, sel.y + sel.height + gap)

    component Sep: Rectangle { implicitWidth: 1; implicitHeight: 24; color: Theme.border }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 2

        Label {
            Layout.leftMargin: 8
            Layout.rightMargin: 6
            Layout.maximumWidth: 150
            elide: Text.ElideMiddle
            text: bar.canvas.selName
            color: Theme.text
        }
        Sep {}
        ToolButtonIcon {
            tip: "Flip horizontally"
            enabled: !bar.canvas.selLocked
            iconPath: "M12 3 V21 M9 6 L3 18 H9 Z M15 6 L21 18 H15 Z"
            onClicked: bar.canvas.flipSelection(true)
        }
        ToolButtonIcon {
            tip: "Flip vertically"
            enabled: !bar.canvas.selLocked
            iconPath: "M3 12 H21 M6 9 L18 3 V9 Z M6 15 L18 21 V15 Z"
            onClicked: bar.canvas.flipSelection(false)
        }
        ToolButtonIcon {
            tip: "Download original file (unchanged)"
            iconPath: "M12 4 V15 M7 10 L12 15 L17 10 M5 19 H19"
            onClicked: bar.downloadRequested()
        }
        ToolButton {
            id: altBtn
            text: "ALT"
            focusPolicy: Qt.NoFocus
            implicitWidth: 44
            implicitHeight: 40
            font.bold: true
            font.pixelSize: 12
            ToolTip.visible: hovered
            ToolTip.text: bar.canvas.selAltText.length > 0 ? "Alt text: " + bar.canvas.selAltText : "Add alt text"
            onClicked: bar.altTextRequested()
            background: Rectangle {
                radius: 8
                color: bar.canvas.selAltText.length > 0 ? Theme.accentSoft : altBtn.hovered ? Theme.hover : "transparent"
            }
            contentItem: Label {
                text: altBtn.text
                font: altBtn.font
                color: bar.canvas.selAltText.length > 0 ? Theme.accent : Theme.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        Sep {}
        ToolButtonIcon {
            tip: bar.canvas.cropping ? "Finish crop (Enter)" : "Crop"
            active: bar.canvas.cropping
            enabled: !bar.canvas.selLocked
            iconPath: "M6 2 V18 H22 M2 6 H18 V22"
            onClicked: bar.canvas.cropping ? bar.canvas.finishCrop() : bar.canvas.startCrop()
        }
        ToolButtonIcon {
            visible: bar.canvas.selCropped && !bar.canvas.cropping
            tip: "Reset crop"
            enabled: !bar.canvas.selLocked
            iconPath: "M4 12 a8 8 0 1 0 2.3 -5.6 M4 4 V9 H9"
            onClicked: bar.canvas.resetCrop()
        }
        ToolButtonIcon {
            tip: "Original size"
            enabled: !bar.canvas.selLocked
            iconPath: "M4 9 V4 H9 M15 4 H20 V9 M20 15 V20 H15 M9 20 H4 V15 M9 9 H15 V15 H9 Z"
            onClicked: bar.canvas.resetImageSize()
        }
        Sep {}
        ToolButtonIcon {
            tip: bar.canvas.selLocked ? "Unlock (Ctrl+Shift+L)" : "Lock (Ctrl+Shift+L)"
            active: bar.canvas.selLocked
            iconPath: bar.canvas.selLocked ? "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 7 0 V11"
                                           : "M6 11 H18 V21 H6 Z M8.5 11 V7.5 a3.5 3.5 0 0 1 6.8 -1.2"
            onClicked: bar.canvas.selLocked = !bar.canvas.selLocked
        }
        ToolButtonIcon {
            id: more
            tip: "More"
            iconPath: "M12 5.5 a1 1 0 1 0 0.01 0 M12 11.5 a1 1 0 1 0 0.01 0 M12 17.5 a1 1 0 1 0 0.01 0"
            onClicked: bar.moreRequested(more)
        }
    }
}
