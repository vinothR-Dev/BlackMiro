import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Miru

// Icon library: search + categories. Click an icon, then click on the board.
Rectangle {
    id: panel

    required property BoardCanvas canvas
    signal closed()

    width: 300
    radius: 14
    color: Theme.surface
    border.color: Theme.border

    readonly property var all: canvas.iconLibrary()
    property string query: ""

    function matches(e) {
        if (query.length === 0) return true
        const q = query.toLowerCase()
        return e.name.toLowerCase().indexOf(q) >= 0 || e.keywords.toLowerCase().indexOf(q) >= 0
                || e.category.toLowerCase().indexOf(q) >= 0
    }
    function categories() {
        const out = []
        for (const e of all)
            if (out.indexOf(e.category) < 0) out.push(e.category)
        return out
    }

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
            Label { text: qsTr("Icons"); font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
            ToolButtonIcon { implicitWidth: 32; implicitHeight: 32; tip: qsTr("Close"); iconPath: "M6 6 L18 18 M18 6 L6 18"; onClicked: panel.closed() }
        }
        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Search icons")
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
                spacing: 10
                Repeater {
                    model: panel.categories()
                    delegate: Column {
                        id: section
                        required property string modelData
                        readonly property var entries: panel.all.filter(e => e.category === modelData && panel.matches(e))
                        visible: entries.length > 0
                        width: parent.width
                        spacing: 4
                        Label { text: section.modelData; font.weight: Font.DemiBold; color: Theme.text }
                        Grid {
                            columns: 6
                            spacing: 4
                            Repeater {
                                model: section.entries
                                delegate: ToolButton {
                                    required property var modelData
                                    implicitWidth: 42; implicitHeight: 42
                                    focusPolicy: Qt.NoFocus
                                    ToolTip.visible: hovered
                                    ToolTip.text: modelData.name
                                    readonly property bool current: panel.canvas.tool === "icon:" + modelData.id
                                    onClicked: { panel.canvas.tool = "icon:" + modelData.id; panel.canvas.forceActiveFocus() }
                                    background: Rectangle {
                                        radius: 8
                                        color: parent.current ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent"
                                    }
                                    contentItem: Image {
                                        source: "image://icons/" + parent.modelData.id + "?stroke=" + String(Theme.text).replace("#", "")
                                        sourceSize: Qt.size(56, 56)
                                        fillMode: Image.PreserveAspectFit
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
