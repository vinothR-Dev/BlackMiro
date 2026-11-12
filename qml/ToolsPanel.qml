import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Miru

// "+" panel: every tool, searchable, grouped, each with "Pin to toolbar".
Popup {
    id: panel

    signal toolActivated(string id)

    width: 340
    height: Math.min(560, parent ? parent.height - 40 : 560)
    padding: 12
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string query: ""
    onOpened: { query = ""; search.text = ""; search.forceActiveFocus() }

    function groups() {
        const out = []
        for (const t of ToolRegistry.tools)
            if (out.indexOf(t.group) < 0) out.push(t.group)
        return out
    }
    function matches(t) {
        const q = query.toLowerCase()
        return q.length === 0 || t.name.toLowerCase().indexOf(q) >= 0 || t.desc.toLowerCase().indexOf(q) >= 0
    }

    contentItem: ColumnLayout {
        spacing: 10
        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Search tools…")
            onTextChanged: panel.query = text
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            Column {
                width: scroll.availableWidth
                spacing: 6
                Repeater {
                    model: panel.groups()
                    delegate: Column {
                        id: group
                        required property string modelData
                        readonly property var entries: ToolRegistry.tools.filter(t => t.group === modelData && panel.matches(t))
                        visible: entries.length > 0
                        width: parent.width
                        spacing: 2
                        Label { text: group.modelData; color: Theme.mutedText; font.pixelSize: 12; font.weight: Font.DemiBold; topPadding: 6; leftPadding: 6 }
                        Repeater {
                            model: group.entries
                            delegate: ItemDelegate {
                                id: row
                                required property var modelData
                                readonly property bool pinned: AppState.pinnedTools.indexOf(modelData.id) >= 0
                                width: group.width
                                implicitHeight: 50
                                padding: 6
                                hoverEnabled: true
                                onClicked: { panel.close(); panel.toolActivated(modelData.id) }
                                background: Rectangle { radius: 8; color: row.hovered ? Theme.hover : "transparent" }
                                contentItem: RowLayout {
                                    spacing: 10
                                    Rectangle {
                                        implicitWidth: 34; implicitHeight: 34; radius: 8
                                        color: Theme.surfaceAlt
                                        Shape {
                                            anchors.centerIn: parent
                                            width: 24; height: 24
                                            scale: 0.85
                                            preferredRendererType: Shape.CurveRenderer
                                            ShapePath {
                                                strokeColor: Theme.text; strokeWidth: 1.8; fillColor: "transparent"
                                                capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                                                PathSvg { path: row.modelData.path }
                                            }
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Label { text: row.modelData.name; color: Theme.text; font.weight: Font.Medium; elide: Text.ElideRight; Layout.fillWidth: true }
                                        Label { text: row.modelData.desc; color: Theme.mutedText; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                                    }
                                    ToolButtonIcon {
                                        implicitWidth: 32; implicitHeight: 32
                                        visible: row.hovered || row.pinned
                                        active: row.pinned
                                        tip: row.pinned ? qsTr("Unpin from toolbar") : qsTr("Pin to toolbar")
                                        iconPath: "M9 3 H15 L14 9 L18 13 H6 L10 9 Z M12 13 V21"
                                        onClicked: AppState.setToolPinned(row.modelData.id, !row.pinned)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
