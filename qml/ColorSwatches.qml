import QtQuick
import QtQuick.Controls
import Miru

// Grid of colour chips. Emits picked(color); "transparent" is shown as a crossed chip.
Grid {
    id: root

    property var colors: []
    property color current: "transparent"
    signal picked(color c)

    columns: 6
    spacing: 6

    Repeater {
        model: root.colors
        delegate: Rectangle {
            required property var modelData
            readonly property bool isClear: Qt.color(modelData).a === 0
            width: 26
            height: 26
            radius: 13
            color: isClear ? "white" : modelData
            border.width: Qt.colorEqual(root.current, modelData) ? 2.5 : 1
            border.color: Qt.colorEqual(root.current, modelData) ? Theme.accent : Theme.border

            Rectangle {
                visible: parent.isClear
                anchors.centerIn: parent
                width: 2
                height: 22
                rotation: 45
                color: "#e5484d"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.picked(parent.modelData)
            }
        }
    }
}
