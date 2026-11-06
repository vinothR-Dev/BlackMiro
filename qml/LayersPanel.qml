import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlackMiro

// Layers: new items go onto the active layer; hide or lock a layer to keep it out of the way.
Rectangle {
    id: panel

    required property BoardCanvas canvas
    signal closed()

    width: 360
    radius: 14
    color: Theme.surface
    border.color: Theme.border

    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.margins: -1
        radius: 15
        color: Qt.alpha(Theme.dark ? "#000000" : "#1f2a44", Theme.dark ? 0.35 : 0.08)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Layers"); font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
            ToolButtonIcon { implicitWidth: 32; implicitHeight: 32; tip: qsTr("Close"); iconPath: "M6 6 L18 18 M18 6 L6 18"; onClicked: panel.closed() }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.subText
            font.pixelSize: 12
            text: qsTr("Top layers are drawn above lower ones. Hidden layers aren't drawn or exported; locked layers can't be selected.")
        }
        Button {
            Layout.fillWidth: true
            highlighted: true
            text: qsTr("New layer")
            onClicked: panel.canvas.addLayer("")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            // Topmost layer first, default layer at the bottom.
            model: panel.canvas.layers.slice().reverse()
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool active: panel.canvas.activeLayer === modelData.id
                readonly property bool isDefault: modelData.id === ""
                width: ListView.view.width
                height: 48
                radius: 10
                color: active ? Theme.accentSoft : hover.hovered ? Theme.hover : "transparent"
                border.width: active ? 1.5 : 0
                border.color: Theme.accent
                HoverHandler { id: hover }
                TapHandler { onTapped: panel.canvas.activeLayer = row.modelData.id; onDoubleTapped: if (!row.isDefault) nameEdit.visible = true }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 4
                    spacing: 2
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            visible: !nameEdit.visible
                            text: row.modelData.name
                            elide: Text.ElideRight
                            color: row.modelData.visible ? Theme.text : Theme.mutedText
                            font.weight: row.active ? Font.DemiBold : Font.Normal
                        }
                        TextField {
                            id: nameEdit
                            visible: false
                            Layout.fillWidth: true
                            implicitHeight: 30
                            text: row.modelData.name
                            onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                            onAccepted: { panel.canvas.renameLayer(row.modelData.id, text); visible = false }
                            onActiveFocusChanged: if (!activeFocus) visible = false
                        }
                        Label {
                            text: qsTr("%n item(s)", "", row.modelData.count) + (row.active ? qsTr(" · new items go here") : "")
                            font.pixelSize: 11
                            color: Theme.mutedText
                        }
                    }
                    ToolButtonIcon {
                        visible: !row.isDefault
                        implicitWidth: 28; implicitHeight: 28
                        tip: qsTr("Move up")
                        iconPath: "M12 18 V6 M7 11 L12 6 L17 11"
                        onClicked: panel.canvas.moveLayer(row.modelData.id, 1)
                    }
                    ToolButtonIcon {
                        visible: !row.isDefault
                        implicitWidth: 28; implicitHeight: 28
                        tip: qsTr("Move down")
                        iconPath: "M12 6 V18 M7 13 L12 18 L17 13"
                        onClicked: panel.canvas.moveLayer(row.modelData.id, -1)
                    }
                    ToolButtonIcon {
                        visible: !row.isDefault
                        implicitWidth: 28; implicitHeight: 28
                        tip: row.modelData.visible ? qsTr("Hide layer") : qsTr("Show layer")
                        active: !row.modelData.visible
                        iconPath: row.modelData.visible
                                  ? "M2 12 C5 6.5 8.5 4.5 12 4.5 C15.5 4.5 19 6.5 22 12 C19 17.5 15.5 19.5 12 19.5 C8.5 19.5 5 17.5 2 12 Z M12 9 a3 3 0 1 0 0.01 0"
                                  : "M3 3 L21 21 M10.5 5 C11 4.7 11.5 4.5 12 4.5 C15.5 4.5 19 6.5 22 12 C21.2 13.4 20.3 14.6 19.3 15.5 M6.5 6.5 C4.8 7.8 3.3 9.6 2 12 C5 17.5 8.5 19.5 12 19.5 C13.5 19.5 15 19.1 16.4 18.3"
                        onClicked: panel.canvas.setLayerVisible(row.modelData.id, !row.modelData.visible)
                    }
                    ToolButtonIcon {
                        visible: !row.isDefault
                        implicitWidth: 28; implicitHeight: 28
                        tip: row.modelData.locked ? qsTr("Unlock layer") : qsTr("Lock layer")
                        active: row.modelData.locked
                        iconPath: row.modelData.locked ? "M5 11 H19 V21 H5 Z M8 11 V7.5 a4 4 0 0 1 8 0 V11"
                                                       : "M5 11 H19 V21 H5 Z M8 11 V7.5 a4 4 0 0 1 7.8 -1.2"
                        onClicked: panel.canvas.setLayerLocked(row.modelData.id, !row.modelData.locked)
                    }
                    ToolButtonIcon {
                        visible: !row.isDefault
                        implicitWidth: 28; implicitHeight: 28
                        tip: qsTr("Delete layer (its items move to the default layer)")
                        iconColor: Theme.danger
                        iconPath: "M5 7 H19 M10 7 V4 H14 V7 M7 7 L8 20 H16 L17 7"
                        onClicked: panel.canvas.deleteLayer(row.modelData.id)
                    }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            enabled: panel.canvas.selectionCount > 0
            text: qsTr("Move selection to active layer")
            onClicked: panel.canvas.moveSelectionToLayer(panel.canvas.activeLayer)
        }
    }
}
