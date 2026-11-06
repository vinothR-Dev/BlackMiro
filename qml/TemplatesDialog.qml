import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlackMiro

// Template gallery opened from the board toolbar: search + use-case categories.
Dialog {
    id: dlg

    required property BoardCanvas canvas
    signal chosen(string id)

    title: qsTr("Templates")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(980, parent ? parent.width - 48 : 980)
    height: Math.min(680, parent ? parent.height - 48 : 680)
    padding: 0
    topPadding: 0

    property var all: canvas.templates()
    property string category: ""
    property string query: ""

    onOpened: { all = canvas.templates(); query = ""; search.text = ""; category = "" }

    function categories() {
        const out = []
        for (const t of all)
            if (out.indexOf(t.category) < 0) out.push(t.category)
        return out
    }
    function visibleTemplates() {
        const q = query.toLowerCase()
        return all.filter(t => (category.length === 0 || t.category === category)
                               && (q.length === 0 || t.name.toLowerCase().indexOf(q) >= 0 || t.description.toLowerCase().indexOf(q) >= 0))
    }

    contentItem: RowLayout {
        spacing: 0

        // Left: categories
        ColumnLayout {
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 2
            component Nav: ItemDelegate {
                property string value
                Layout.fillWidth: true
                implicitHeight: 34
                highlighted: dlg.category === value
                background: Rectangle { radius: 8; color: parent.highlighted ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent" }
                contentItem: Label { text: parent.text; color: parent.highlighted ? Theme.accent : Theme.text; font.weight: parent.highlighted ? Font.DemiBold : Font.Normal; elide: Text.ElideRight }
                onClicked: dlg.category = value
            }
            Nav { text: qsTr("All templates"); value: "" }
            Label { text: qsTr("Use cases"); color: Theme.mutedText; font.pixelSize: 12; Layout.topMargin: 12; Layout.leftMargin: 10 }
            Repeater {
                model: dlg.categories()
                delegate: Nav { required property string modelData; text: modelData; value: modelData }
            }
            Item { Layout.fillHeight: true }
        }
        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Theme.border }

        // Right: search + grid
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 12
            TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search templates")
                onTextChanged: dlg.query = text
            }
            ScrollView {
                id: scroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                Flow {
                    width: scroll.availableWidth
                    spacing: 14
                    readonly property int cols: Math.max(1, Math.floor((width + 14) / 230))
                    readonly property real cardW: (width - (cols - 1) * 14) / cols
                    Repeater {
                        model: [{ id: "blank", name: qsTr("Blank board"), description: qsTr("Start from an empty canvas") }].concat(dlg.visibleTemplates())
                        delegate: ItemDelegate {
                            id: card
                            required property var modelData
                            width: parent.cardW
                            height: 190
                            padding: 0
                            hoverEnabled: true
                            onClicked: { dlg.close(); dlg.chosen(modelData.id) }
                            background: Rectangle {
                                radius: 12
                                color: Theme.surface
                                border.width: card.hovered || card.visualFocus ? 1.5 : 1
                                border.color: card.hovered || card.visualFocus ? Theme.accent : Theme.border
                            }
                            contentItem: ColumnLayout {
                                spacing: 0
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 118
                                    Layout.margins: 6
                                    radius: 8
                                    color: "#f5f6f8"
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        visible: card.modelData.id !== "blank"
                                        source: card.modelData.id !== "blank" ? "image://templates/" + card.modelData.id : ""
                                        sourceSize: Qt.size(Math.round(width * 2), Math.round(height * 2))
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                    }
                                    Label { anchors.centerIn: parent; visible: card.modelData.id === "blank"; text: "+"; font.pixelSize: 34; color: Theme.accent }
                                }
                                Label { Layout.leftMargin: 12; Layout.rightMargin: 12; Layout.fillWidth: true; text: card.modelData.name; font.weight: Font.DemiBold; color: Theme.text; elide: Text.ElideRight }
                                Label { Layout.leftMargin: 12; Layout.rightMargin: 12; Layout.fillWidth: true; Layout.bottomMargin: 8; text: card.modelData.description; font.pixelSize: 12; color: Theme.mutedText; elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }
        }
    }
    footer: null
}
