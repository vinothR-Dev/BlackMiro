import QtQuick
import QtQuick.Controls
import BlackMiro

// Top-right theme switch: one icon button. Each click moves to the next theme
// (System -> Light -> Dark) and the icon changes to match.
ToolButtonIcon {
    id: root

    readonly property var order: ["system", "light", "dark"]
    readonly property var icons: ({
        // monitor, sun, moon
        system: "M3 5 H21 V16 H3 Z M9 20 H15 M12 16 V20",
        light: "M12 8 a4 4 0 1 0 0.01 0 M12 2 V4 M12 20 V22 M2 12 H4 M20 12 H22 M4.9 4.9 L6.3 6.3 M17.7 17.7 L19.1 19.1 M4.9 19.1 L6.3 17.7 M17.7 6.3 L19.1 4.9",
        dark: "M20 14.5 A8 8 0 1 1 9.5 4 A6.5 6.5 0 0 0 20 14.5 Z"
    })
    readonly property var names: ({ system: qsTr("System"), light: qsTr("Light"), dark: qsTr("Dark") })
    readonly property string next: order[(order.indexOf(AppState.themeMode) + 1) % order.length]

    tip: qsTr("Theme: %1 (click for %2)").arg(names[AppState.themeMode]).arg(names[next])
    Accessible.name: tip
    iconPath: icons[AppState.themeMode]
    onClicked: AppState.themeMode = next
}
