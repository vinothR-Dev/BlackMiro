pragma Singleton
import QtQuick
import BlackMiro

// All UI colours in one place. AppState.dark follows the theme button
// (System / Light / Dark); "System" tracks the operating system.
QtObject {
    readonly property bool dark: AppState.dark

    readonly property color canvas:      dark ? "#15171c" : "#f5f6f8"
    readonly property color grid:        dark ? "#353a45" : "#c8ccd4"
    readonly property color surface:     dark ? "#23262e" : "#ffffff"
    readonly property color surfaceAlt:  dark ? "#2a2e37" : "#f5f6f8"
    readonly property color border:      dark ? "#373c47" : "#e3e5e9"
    readonly property color text:        dark ? "#e9ebf0" : "#1f2329"
    readonly property color subText:     dark ? "#a4aab6" : "#5b6170"
    readonly property color mutedText:   dark ? "#7f8693" : "#8a8f99"
    readonly property color hover:       dark ? "#2f343e" : "#f1f2f5"
    readonly property color pressed:     dark ? "#3a404b" : "#e4e6ea"
    readonly property color accent:      dark ? "#6f88ff" : "#4262ff"
    readonly property color accentHover: dark ? "#8197ff" : "#3655f5"
    readonly property color accentSoft:  dark ? "#2c3459" : "#e8ecff"
    readonly property color accentText:  "#ffffff"
    readonly property color danger:      dark ? "#ff6b6f" : "#e5484d"
    readonly property color success:     dark ? "#4cc38a" : "#0c7a43"
    readonly property color toast:       dark ? "#3a3f4b" : "#1f2329"
    readonly property color selection:   dark ? "#3d4a8a" : "#b3c1ff"
    readonly property color iconDisabled: dark ? "#555b67" : "#b4b7bf"
}
