import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// "Saved files": images and selections kept on this device for reuse on any board.
Dialog {
    id: dlg

    required property BoardCanvas canvas
    property var files: []

    title: qsTr("Saved files")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, parent ? parent.width - 32 : 640)
    height: Math.min(520, parent ? parent.height - 32 : 520)
    padding: 16
    standardButtons: Dialog.Close

    onOpened: files = canvas.savedFiles()

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.subText
            text: dlg.files.length > 0
                  ? qsTr("Click a file to add it to the board.")
                  : qsTr("Nothing saved yet. Right-click an image or a selection and choose \"Add to Saved files\".")
        }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: Math.floor(width / Math.max(1, Math.floor(width / 150)))
            cellHeight: 150
            model: dlg.files
            delegate: Item {
                id: cell
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 10
                    color: hover.hovered ? Theme.hover : Theme.surfaceAlt
                    border.color: Theme.border
                    HoverHandler { id: hover }
                    TapHandler {
                        onTapped: {
                            if (dlg.canvas.insertSavedFile(cell.modelData.url))
                                dlg.close()
                        }
                    }
                    Image {
                        anchors { fill: parent; margins: 8; bottomMargin: 28 }
                        source: cell.modelData.url
                        sourceSize: Qt.size(240, 240)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Label {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
                        text: cell.modelData.name
                        elide: Text.ElideMiddle
                        color: Theme.subText
                        font.pixelSize: 12
                    }
                    ToolButton {
                        anchors { top: parent.top; right: parent.right; margins: 2 }
                        visible: hover.hovered
                        text: "✕"
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove from Saved files")
                        onClicked: {
                            dlg.canvas.removeSavedFile(cell.modelData.url)
                            dlg.files = dlg.canvas.savedFiles()
                        }
                    }
                }
            }
        }
    }
}
