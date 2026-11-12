import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 8
    leftPadding: 12
    rightPadding: 12 + (indicator ? indicator.width + spacing : 0)
    spacing: 8
    hoverEnabled: true

    delegate: T.ItemDelegate {
        id: row
        required property var model
        required property int index
        width: ListView.view.width
        implicitHeight: 34
        padding: 7
        horizontalPadding: 12
        hoverEnabled: true
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: row.model[control.textRole]
            font.pixelSize: control.font.pixelSize
            font.weight: control.currentIndex === row.index ? Font.DemiBold : Font.Normal
            color: control.currentIndex === row.index ? control.palette.highlight : control.palette.windowText
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 7
            color: row.highlighted ? control.palette.midlight : "transparent"
        }
    }

    indicator: Shape {
        x: control.width - width - 12
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 14; height: 14
        preferredRendererType: Shape.CurveRenderer
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: 140 } }
        ShapePath {
            strokeColor: control.palette.placeholderText
            strokeWidth: 1.7
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M3 5.5 L7 9.5 L11 5.5" }
        }
    }

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.palette.text
        opacity: control.enabled ? 1 : 0.45
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 180
        implicitHeight: 40
        radius: 8
        color: control.palette.base
        border.width: control.visualFocus || control.popup.visible ? 1.5 : 1
        border.color: control.visualFocus || control.popup.visible ? control.palette.highlight
                    : control.hovered ? control.palette.placeholderText : control.palette.mid
    }

    popup: T.Popup {
        y: control.height + 6
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                         control.Window.height - topMargin - bottomMargin)
        topMargin: 8
        bottomMargin: 8
        padding: 6

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 } }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
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
    }
}
