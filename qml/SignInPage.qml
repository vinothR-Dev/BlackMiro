import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import BlackMiro

// Email-only, password-less sign-in / sign-up, shown only when an action needs it
// (saving a board, sharing). There is no server: the email is a local profile.
Rectangle {
    id: page

    property string reason: ""                 // e.g. "Sign in to save your board"
    property bool signUp: AppState.lastEmail.length === 0
    signal dismissed()

    color: Theme.canvas

    function submit() {
        if (!AppState.isValidEmail(email.text)) {
            hint.errorText = qsTr("Enter a valid email address, like name@example.com.")
            email.forceActiveFocus()
            return
        }
        hint.errorText = ""
        AppState.signIn(email.text, remember.checked)
    }

    onVisibleChanged: if (visible) { hint.errorText = ""; email.forceActiveFocus() }

    // Dotted board backdrop (static, painted once per resize/theme change)
    Canvas {
        anchors.fill: parent
        property color dot: Theme.grid
        onDotChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = dot
            for (let x = 12; x < width; x += 24)
                for (let y = 12; y < height; y += 24)
                    ctx.fillRect(x, y, 1.5, 1.5)
        }
    }

    // Eat clicks so the board underneath can't be used through the overlay.
    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: (w) => w.accepted = true }
    Keys.onEscapePressed: page.dismissed()

    component Logo: Rectangle {
        implicitWidth: 34
        implicitHeight: 34
        radius: 10
        color: Theme.text
        Shape {
            anchors.centerIn: parent
            width: 24; height: 24
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.surface
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M4 4 H11 V11 H4 Z M17.5 13 a3.5 3.5 0 1 0 0.01 0 M11 11 L15 14.5 M4 20 H20" }
            }
        }
    }

    // ---- Top bar
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
        spacing: 10
        Logo {}
        Label {
            text: "BlackMiro"
            font.pixelSize: 19
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Item { Layout.fillWidth: true }
        ThemeButton {}
        ToolButtonIcon {
            tip: qsTr("Close — keep working without signing in (Esc)")
            iconPath: "M6 6 L18 18 M18 6 L6 18"
            onClicked: page.dismissed()
        }
    }

    // ---- Card
    Item {
        anchors.centerIn: parent
        width: Math.min(440, page.width - 32)
        height: form.implicitHeight + 72

        Rectangle {   // soft shadow
            anchors.fill: parent
            anchors.topMargin: 14
            anchors.margins: -2
            radius: 22
            color: Qt.alpha(Theme.dark ? "#000000" : "#1f2a44", Theme.dark ? 0.35 : 0.08)
        }
        Rectangle {
            anchors.fill: parent
            radius: 20
            color: Theme.surface
            border.color: Theme.border
        }

        ColumnLayout {
            id: form
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 36 }
            spacing: 0

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: page.signUp ? qsTr("Create your BlackMiro space") : qsTr("Welcome back")
                font.pixelSize: 26
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 8
                wrapMode: Text.WordWrap
                lineHeight: 1.35
                color: Theme.subText
                font.pixelSize: 15
                text: page.reason.length > 0
                      ? page.reason
                      : page.signUp ? qsTr("Sign up with just your email. Every feature is free.")
                                    : qsTr("Sign in with your email to save and share your boards.")
            }

            Label {
                Layout.topMargin: 26
                text: qsTr("Email")
                font.weight: Font.Medium
                color: Theme.text
            }
            TextField {
                id: email
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.preferredHeight: 44
                font.pixelSize: 15
                placeholderText: "name@example.com"
                text: AppState.lastEmail
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                onAccepted: page.submit()
                Accessible.name: qsTr("Email")
            }
            Label {
                id: hint
                property string errorText: ""
                Layout.fillWidth: true
                Layout.topMargin: 8
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: errorText.length > 0 ? Theme.danger : Theme.mutedText
                text: errorText.length > 0 ? errorText : qsTr("No password required.")
            }
            CheckBox {
                id: remember
                Layout.topMargin: 14
                text: qsTr("Remember me on this device")
                checked: AppState.rememberMe
            }
            Button {
                Layout.fillWidth: true
                Layout.topMargin: 18
                Layout.preferredHeight: 46
                highlighted: true
                font.pixelSize: 15
                text: qsTr("Continue with email")
                onClicked: page.submit()
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 18
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.StyledText
                color: Theme.subText
                linkColor: Theme.accent
                text: page.signUp ? qsTr("Already used BlackMiro here? <a href='x'>Sign in</a>")
                                  : qsTr("New here? <a href='x'>Create a space</a>")
                onLinkActivated: page.signUp = !page.signUp
                HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 22
                implicitHeight: privacy.implicitHeight + 24
                radius: 10
                color: Theme.surfaceAlt
                Label {
                    id: privacy
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 }
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                    color: Theme.mutedText
                    font.pixelSize: 12
                    text: qsTr("Your profile stays on this device. No account server, no verification codes, no tracking or cookies.")
                }
            }
        }
    }
}
