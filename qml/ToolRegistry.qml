pragma Singleton
import QtQuick

// Every board tool: id, label, description, icon (24×24 SVG path) and group.
// The toolbar shows the pinned ones; the "+" panel lists them all with "Pin to toolbar".
QtObject {
    readonly property var tools: [
        { id: "templates", name: qsTr("Templates"), group: qsTr("Essentials"), desc: qsTr("Start faster from a ready-made board"),
          path: "M4 4 H20 V20 H4 Z M4 9 H20 M9 9 V20" },
        { id: "sticky", name: qsTr("Sticky note"), group: qsTr("Essentials"), desc: qsTr("16 colours, stack mode (N)"),
          path: "M4 4 H20 V14 L14 20 H4 Z M14 20 V14 H20" },
        { id: "text", name: qsTr("Text"), group: qsTr("Essentials"), desc: qsTr("Add a text box (T)"),
          path: "M5 7 V4 H19 V7 M12 4 V20 M9 20 H15" },
        { id: "shapes", name: qsTr("Shapes and lines"), group: qsTr("Essentials"), desc: qsTr("Lines, arrows and basic shapes"),
          path: "M3 13 H10 V20 H3 Z M17 13.5 a3.5 3.5 0 1 0 0.01 0 M10 3 L14 10 H6 Z" },
        { id: "pen", name: qsTr("Freehand drawing"), group: qsTr("Essentials"), desc: qsTr("Pen, highlighter, smart drawing, erasers, lasso (P)"),
          path: "M4 20 L8 19 L19 8 L16 5 L5 16 Z M14 7 L17 10" },
        { id: "frame", name: qsTr("Frame"), group: qsTr("Essentials"), desc: qsTr("Sections with size presets: A4, 16:9, phone… (F)"),
          path: "M7 3 V21 M17 3 V21 M3 7 H21 M3 17 H21" },
        { id: "connector", name: qsTr("Connection line"), group: qsTr("Essentials"), desc: qsTr("Arrow that sticks to shapes (L)"),
          path: "M5 19 L19 5 M13 5 H19 V11" },
        { id: "emoji", name: qsTr("Stickers and emoji"), group: qsTr("Media"), desc: qsTr("Reactions and emoji stickers"),
          path: "M12 3 a9 9 0 1 0 0.01 0 M8.5 14.5 C10.5 16.5 13.5 16.5 15.5 14.5 M9 10 V10.5 M15 10 V10.5" },
        { id: "image", name: qsTr("Upload image"), group: qsTr("Media"), desc: qsTr("Insert pictures (or drop / paste them)"),
          path: "M4 5 H20 V19 H4 Z M4 16 L9 11 L13 15 L16 12 L20 16 M15.5 8.5 a1.5 1.5 0 1 0 0.01 0" },
        { id: "icons", name: qsTr("Icons"), group: qsTr("Media"), desc: qsTr("70+ line icons in 6 categories"),
          path: "M12 3 L14.6 8.9 L21 9.5 L16.2 13.7 L17.6 20 L12 16.8 L6.4 20 L7.8 13.7 L3 9.5 L9.4 8.9 Z" },
        { id: "shapelib", name: qsTr("Shape library"), group: qsTr("Diagramming"), desc: qsTr("Basic, flowchart, callout and arrow shapes"),
          path: "M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M17 14 a3 3 0 1 0 0.01 0" },
        { id: "layers", name: qsTr("Layers"), group: qsTr("Diagramming"), desc: qsTr("Arrange, hide and lock content in layers"),
          path: "M12 3 L21 8 L12 13 L3 8 Z M3 12.5 L12 17.5 L21 12.5 M3 17 L12 22 L21 17" },
        { id: "comment", name: qsTr("Comment"), group: qsTr("Collaboration"), desc: qsTr("Pin a comment anywhere on the board (C)"),
          path: "M4 5 H20 V16 H10 L5 20 V16 H4 Z" },
        { id: "saved", name: qsTr("Saved files"), group: qsTr("Media"), desc: qsTr("Images and selections you saved for reuse"),
          path: "M6 3 H18 V21 L12 16.5 L6 21 Z" },
        { id: "shortcuts", name: qsTr("Keyboard shortcuts"), group: qsTr("Navigation"), desc: qsTr("Every shortcut on one page (F1)"),
          path: "M3 6 H21 V18 H3 Z M6 9 H7 M10 9 H11 M14 9 H15 M18 9 H18.5 M6 12 H7 M10 12 H11 M14 12 H15 M8 15 H16" },
        { id: "bulk", name: qsTr("Bulk mode"), group: qsTr("Planning"), desc: qsTr("Create many sticky notes at once"),
          path: "M8 8 H20 V20 H8 Z M4 16 V4 H16" },
        { id: "lasso", name: qsTr("Lasso select"), group: qsTr("Drawing"), desc: qsTr("Draw around things to select them"),
          path: "M12 4 C6 4 3 7 3 10 C3 13 7 15 12 15 C17 15 21 13 21 10 C21 7 18 4 12 4 Z M7 14 C6 17 8 20 10 20" },
        { id: "eraser", name: qsTr("Eraser"), group: qsTr("Drawing"), desc: qsTr("Erase pen strokes"),
          path: "M7 20 H20 M4 15 L13 6 L19 12 L11 20 H8 Z M9 11 L14 16" },
        { id: "hand", name: qsTr("Hand"), group: qsTr("Navigation"), desc: qsTr("Move around the board (H or hold Space)"),
          path: "M8 12 V6 a1.5 1.5 0 0 1 3 0 V11 M11 10 V4.5 a1.5 1.5 0 0 1 3 0 V11 M14 10 V6 a1.5 1.5 0 0 1 3 0 V14 a7 7 0 0 1 -7 7 a6 6 0 0 1 -5 -3 L3.5 14 a1.5 1.5 0 0 1 2.5 -1.5 L8 15" }
    ]

    readonly property var defaultPinned: ["templates", "sticky", "text", "shapes", "pen", "frame", "connector", "emoji", "image"]

    function find(id) {
        for (const t of tools)
            if (t.id === id) return t
        return null
    }
}
