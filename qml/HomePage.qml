import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import BlackMiro

// Start screen: new board, templates, recent boards, join a live session.
Rectangle {
    id: home

    required property BoardCanvas canvas
    property bool canClose: true
    signal templateChosen(string id)
    signal recentChosen(string path)
    signal openRequested()
    signal joinRequested()
    signal closed()

    color: Theme.canvas
    readonly property bool isWeb: Qt.platform.os === "wasm"
    readonly property real contentWidth: Math.min(1160, width - 48)
    readonly property int cardColumns: Math.max(1, Math.floor((contentWidth + 16) / 236))
    readonly property real cardWidth: (contentWidth - (cardColumns - 1) * 16) / cardColumns

    Keys.onEscapePressed: if (canClose) closed()
    // Block clicks and hover from reaching the board underneath.
    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: (w) => w.accepted = false }

    function greeting() {
        if (!AppState.signedIn)
            return qsTr("Start something new")
        const name = AppState.email.split("@")[0]
        return qsTr("Let's build, %1").arg(name.charAt(0).toUpperCase() + name.slice(1))
    }

    function relativeTime(t) {
        const d = new Date(t)
        const mins = Math.round((Date.now() - d.getTime()) / 60000)
        if (mins < 1) return qsTr("just now")
        if (mins < 60) return qsTr("%1 min ago").arg(mins)
        if (mins < 1440) return qsTr("%1 h ago").arg(Math.round(mins / 60))
        return d.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
    }

    // ---- Top bar
    RowLayout {
        id: topBar
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
        spacing: 10
        Rectangle {
            implicitWidth: 34; implicitHeight: 34; radius: 10
            color: Theme.text
            Shape {
                anchors.centerIn: parent
                width: 24; height: 24
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Theme.surface; strokeWidth: 2; fillColor: "transparent"
                    capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: "M4 4 H11 V11 H4 Z M17.5 13 a3.5 3.5 0 1 0 0.01 0 M11 11 L15 14.5 M4 20 H20" }
                }
            }
        }
        Label { text: "BlackMiro"; font.pixelSize: 19; font.weight: Font.DemiBold; color: Theme.text }
        Item { Layout.fillWidth: true }
        Button { text: qsTr("Join live session"); onClicked: home.joinRequested() }
        Button { visible: !home.isWeb; text: qsTr("Open file…"); onClicked: home.openRequested() }
        ThemeButton {}
        ToolButtonIcon {
            visible: home.canClose
            tip: qsTr("Back to the board (Esc)")
            iconPath: "M6 6 L18 18 M18 6 L6 18"
            onClicked: home.closed()
        }
    }

    ScrollView {
        id: scroll
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; bottom: parent.bottom; topMargin: 12 }
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: (scroll.availableWidth - home.contentWidth) / 2
            width: home.contentWidth
            spacing: 0

            Label {
                Layout.topMargin: 20
                text: home.greeting()
                font.pixelSize: 30
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.topMargin: 6
                text: qsTr("Pick a template or start from a blank board. Everything is free.")
                font.pixelSize: 15
                color: Theme.subText
            }

            // ---- Templates
            Label {
                Layout.topMargin: 28
                text: qsTr("Start something new")
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Flow {
                Layout.topMargin: 12
                Layout.fillWidth: true
                spacing: 16

                Repeater {
                    model: [{ id: "blank", name: qsTr("Blank board"), description: qsTr("An empty infinite canvas") }]
                               .concat(home.canvas.templates())
                    delegate: ItemCard {
                        required property var modelData
                        width: home.cardWidth
                        title: modelData.name
                        subtitle: modelData.description
                        templateId: modelData.id
                        onClicked: home.templateChosen(modelData.id)
                    }
                }
            }

            // ---- Recent boards
            Label {
                Layout.topMargin: 36
                text: qsTr("Recent boards")
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.topMargin: 10
                visible: AppState.recentBoards.length === 0 || home.isWeb
                color: Theme.mutedText
                text: home.isWeb ? qsTr("In the browser, open boards with “Open” in the board menu.")
                                 : qsTr("Boards you save appear here.")
            }
            Flow {
                Layout.topMargin: 12
                Layout.fillWidth: true
                Layout.bottomMargin: 40
                visible: !home.isWeb
                spacing: 16
                Repeater {
                    model: AppState.recentBoards
                    delegate: ItemCard {
                        required property var modelData
                        width: home.cardWidth
                        title: modelData.title
                        subtitle: home.relativeTime(modelData.time)
                        thumb: modelData.thumb
                        removable: true
                        onClicked: home.recentChosen(modelData.path)
                        onRemoveClicked: AppState.removeRecent(modelData.path)
                    }
                }
            }
        }
    }

    // Card used for templates and recent boards.
    component ItemCard: Item {
        id: card
        property string title
        property string subtitle
        property string templateId
        property string thumb
        property bool removable: false
        signal clicked()
        signal removeClicked()

        implicitHeight: 196
        Accessible.role: Accessible.Button
        Accessible.name: title
        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: card.clicked() }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: hover.hovered ? 2 : 4
            anchors.margins: -1
            radius: 15
            color: Qt.alpha(Theme.dark ? "#000000" : "#1f2a44", hover.hovered ? 0.14 : 0.05)
            Behavior on anchors.topMargin { NumberAnimation { duration: 120 } }
        }
        Rectangle {
            anchors.fill: parent
            radius: 14
            color: Theme.surface
            border.width: card.activeFocus ? 2 : 1
            border.color: card.activeFocus || hover.hovered ? Theme.accent : Theme.border
            Behavior on border.color { ColorAnimation { duration: 120 } }

            Rectangle {
                id: preview
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                height: 120
                radius: 9
                color: "#f5f6f8"
                clip: true
                Image {
                    anchors.fill: parent
                    visible: card.templateId.length > 0 && card.templateId !== "blank" || card.thumb.length > 0
                    source: card.thumb.length > 0 ? card.thumb
                          : card.templateId.length > 0 && card.templateId !== "blank" ? "image://templates/" + card.templateId : ""
                    sourceSize: Qt.size(Math.round(width * 2), Math.round(height * 2))
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: card.thumb.length === 0
                }
                Shape {
                    anchors.centerIn: parent
                    visible: card.templateId === "blank"
                    width: 32; height: 32
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: Theme.accent; strokeWidth: 2.4; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                        PathSvg { path: "M16 6 V26 M6 16 H26" }
                    }
                }
            }
            Label {
                id: titleLabel
                anchors { left: parent.left; right: parent.right; top: preview.bottom; topMargin: 10; leftMargin: 14; rightMargin: 14 }
                text: card.title
                elide: Text.ElideRight
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                anchors { left: parent.left; right: parent.right; top: titleLabel.bottom; topMargin: 3; leftMargin: 14; rightMargin: 14 }
                text: card.subtitle
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Theme.mutedText
            }
        }
        ToolButtonIcon {
            visible: card.removable && hover.hovered
            anchors { right: parent.right; top: parent.top; margins: 12 }
            implicitWidth: 30; implicitHeight: 30
            tip: qsTr("Remove from recent")
            iconPath: "M7 7 L17 17 M17 7 L7 17"
            onClicked: card.removeClicked()
        }
    }
}
