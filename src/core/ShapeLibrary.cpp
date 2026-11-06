#include "ShapeLibrary.h"

#include "SvgPath.h"

#include <QHash>
#include <QMutex>
#include <QTransform>

namespace ShapeLibrary {

namespace {

const QString kCircle = QStringLiteral("M0 50A50 50 0 1 0 100 50A50 50 0 1 0 0 50Z");
const QString kCylinder = QStringLiteral("M0 12A50 12 0 0 1 100 12V88A50 12 0 0 1 0 88Z M0 12A50 12 0 0 0 100 12");
const QString kPill = QStringLiteral("M25 0H75A25 50 0 0 1 75 100H25A25 50 0 0 1 25 0Z");
const QString kRhombus = QStringLiteral("M50 0L100 50L50 100L0 50Z");
const QString kRect = QStringLiteral("M0 0H100V100H0Z");

QList<Entry> buildShapes()
{
    const QString B = QStringLiteral("Basic shapes");
    const QString F = QStringLiteral("Flowchart");
    const QString C = QStringLiteral("Callouts");
    const QString A = QStringLiteral("Arrows");
    return {
        // Basic
        { "rect", "Rectangle", B, kRect, "square box" },
        { "rounded", "Rounded rectangle", B, "M12 0H88A12 12 0 0 1 100 12V88A12 12 0 0 1 88 100H12A12 12 0 0 1 0 88V12A12 12 0 0 1 12 0Z", "box card" },
        { "circle", "Circle", B, kCircle, "oval ellipse round" },
        { "triangle", "Triangle", B, "M50 0L100 100H0Z", "" },
        { "right-triangle", "Right triangle", B, "M0 0L100 100H0Z", "" },
        { "rhombus", "Rhombus", B, kRhombus, "diamond decision" },
        { "parallelogram", "Parallelogram", B, "M25 0H100L75 100H0Z", "slanted" },
        { "trapezoid", "Trapezoid", B, "M22 0H78L100 100H0Z", "" },
        { "pentagon", "Pentagon", B, "M50 0L100 38L81 100H19L0 38Z", "" },
        { "hexagon", "Hexagon", B, "M25 0H75L100 50L75 100H25L0 50Z", "" },
        { "octagon", "Octagon", B, "M30 0H70L100 30V70L70 100H30L0 70V30Z", "stop" },
        { "star", "Star", B, "M50 0L61.8 33.8L97.6 34.5L69 56.2L79.4 90.5L50 70L20.6 90.5L31 56.2L2.4 34.5L38.2 33.8Z", "favourite rating" },
        { "cross", "Cross", B, "M33 0H67V33H100V67H67V100H33V67H0V33H33Z", "plus add" },
        { "heart", "Heart", B, "M50 96C20 73 0 56 0 30C0 12 13 0 28 0C40 0 47 7 50 15C53 7 60 0 72 0C87 0 100 12 100 30C100 56 80 73 50 96Z", "love like" },
        { "cloud", "Cloud", B, "M25 88C10 88 0 78 0 64C0 50 10 42 22 41C22 24 36 12 53 12C66 12 77 20 81 32C93 33 100 44 100 57C100 74 90 88 75 88Z", "weather" },
        { "cylinder", "Cylinder", B, kCylinder, "can database" },
        { "chevron", "Chevron", B, "M0 0H70L100 50L70 100H0L30 50Z", "step" },
        { "pill", "Pill", B, kPill, "stadium capsule" },
        { "speech", "Speech bubble", B, "M10 0H90A10 10 0 0 1 100 10V65A10 10 0 0 1 90 75H40L18 98L22 75H10A10 10 0 0 1 0 65V10A10 10 0 0 1 10 0Z", "comment chat" },
        // Flowchart
        { "fc-process", "Process", F, kRect, "step action" },
        { "fc-decision", "Decision", F, kRhombus, "if condition" },
        { "fc-terminator", "Start / end", F, kPill, "terminal begin finish" },
        { "fc-data", "Data (input/output)", F, "M20 0H100L80 100H0Z", "io" },
        { "fc-document", "Document", F, "M0 0H100V82C75 66 55 100 25 92C14 89 6 85 0 82Z", "page report" },
        { "fc-multidoc", "Multiple documents", F, "M10 0H100V72H90 M0 10H90V82C68 68 52 100 25 92C14 89 6 85 0 82Z", "pages" },
        { "fc-predefined", "Predefined process", F, "M0 0H100V100H0Z M12 0V100 M88 0V100", "subroutine function" },
        { "fc-manual-input", "Manual input", F, "M0 25L100 0V100H0Z", "keyboard" },
        { "fc-manual-op", "Manual operation", F, "M0 0H100L80 100H20Z", "" },
        { "fc-preparation", "Preparation", F, "M20 0H80L100 50L80 100H20L0 50Z", "setup" },
        { "fc-delay", "Delay", F, "M0 0H55A45 50 0 0 1 55 100H0Z", "wait" },
        { "fc-database", "Database", F, kCylinder, "storage db" },
        { "fc-stored-data", "Stored data", F, "M20 0H100A20 50 0 0 0 100 100H20A20 50 0 0 1 20 0Z", "" },
        { "fc-display", "Display", F, "M22 0H78A22 50 0 0 1 78 100H22L0 50Z", "screen" },
        { "fc-offpage", "Off-page connector", F, "M0 0H100V65L50 100L0 65Z", "" },
        { "fc-connector", "Connector", F, kCircle, "junction" },
        { "fc-merge", "Merge", F, "M0 0H100L50 100Z", "" },
        { "fc-extract", "Extract", F, "M50 0L100 100H0Z", "" },
        { "fc-sort", "Sort", F, "M50 0L100 50L50 100L0 50Z M0 50H100", "" },
        { "fc-card", "Card", F, "M22 0H100V100H0V22Z", "punch" },
        { "fc-internal", "Internal storage", F, "M0 0H100V100H0Z M15 0V100 M0 15H100", "memory" },
        { "fc-loop", "Loop limit", F, "M20 0H80L100 22V100H0V22Z", "" },
        { "fc-or", "Or", F, kCircle + " M50 0V100 M0 50H100", "" },
        { "fc-summing", "Summing junction", F, kCircle + " M15 15L85 85 M85 15L15 85", "" },
        { "fc-collate", "Collate", F, "M0 0H100L0 100H100Z", "hourglass" },
        // Callouts
        { "callout-rect", "Callout", C, "M0 0H100V70H45L25 100L28 70H0Z", "note label" },
        { "callout-rounded", "Rounded callout", C, "M10 0H90A10 10 0 0 1 100 10V60A10 10 0 0 1 90 70H45L25 100L28 70H10A10 10 0 0 1 0 60V10A10 10 0 0 1 10 0Z", "speech" },
        { "callout-oval", "Oval callout", C, "M50 0A50 35 0 1 1 34 68L14 96L20 62A50 35 0 0 1 50 0Z", "speech" },
        { "callout-cloud", "Thought cloud", C, "M25 70C12 70 3 62 3 51C3 40 11 34 21 33C21 19 33 9 47 9C58 9 67 16 70 26C80 27 86 36 86 46C86 60 78 70 66 70Z M24 84A6 6 0 1 0 36 84A6 6 0 1 0 24 84Z M12 95A4 4 0 1 0 20 95A4 4 0 1 0 12 95Z", "think" },
        { "callout-down", "Label", C, "M0 0H100V70H60L50 100L40 70H0Z", "pin pointer" },
        // Arrows
        { "arrow-right", "Block arrow right", A, "M0 28H62V5L100 50L62 95V72H0Z", "" },
        { "arrow-left", "Block arrow left", A, "M100 28H38V5L0 50L38 95V72H100Z", "" },
        { "arrow-up", "Block arrow up", A, "M28 100V38H5L50 0L95 38H72V100Z", "" },
        { "arrow-down", "Block arrow down", A, "M28 0V62H5L50 100L95 62H72V0Z", "" },
        { "arrow-double", "Double arrow", A, "M0 50L26 12V32H74V12L100 50L74 88V68H26V88Z", "both ways" },
        { "arrow-chevron", "Chevron arrow", A, "M0 0H70L100 50L70 100H0L30 50Z", "process step" },
        { "arrow-pentagon", "Pentagon arrow", A, "M0 0H72L100 50L72 100H0Z", "tag step" },
    };
}

QList<Entry> buildIcons()
{
    const QString I = QStringLiteral("Interface");
    const QString R = QStringLiteral("Arrows");
    const QString M = QStringLiteral("Communication");
    const QString O = QStringLiteral("Objects");
    const QString S = QStringLiteral("Symbols");
    const QString T = QStringLiteral("Technology");
    const QString circle = QStringLiteral("M12 3 a9 9 0 1 0 0.01 0");
    return {
        { "home", "Home", I, "M3 11 L12 3 L21 11 M5 9.5 V21 H19 V9.5 M10 21 V15 H14 V21", "house" },
        { "search", "Search", I, "M10.5 3.5 a7 7 0 1 0 0.01 0 M15.5 15.5 L21 21", "find magnifier" },
        { "sliders", "Settings", I, "M4 6 H20 M4 12 H20 M4 18 H20 M8 4 V8 M15 10 V14 M10 16 V20", "options preferences" },
        { "check", "Check", I, "M4 12.5 L9.5 18 L20 6", "done ok tick" },
        { "close", "Close", I, "M6 6 L18 18 M18 6 L6 18", "x cancel" },
        { "plus", "Plus", I, "M12 4 V20 M4 12 H20", "add new" },
        { "minus", "Minus", I, "M4 12 H20", "remove" },
        { "menu", "Menu", I, "M4 6 H20 M4 12 H20 M4 18 H20", "hamburger" },
        { "user", "User", I, "M12 3 a4.5 4.5 0 1 0 0.01 0 M3.5 21 a8.5 7 0 0 1 17 0", "person profile" },
        { "users", "Team", I, "M9 4 a3.5 3.5 0 1 0 0.01 0 M2 20 a7 6 0 0 1 14 0 M16 4.5 a3 3 0 1 1 0 6 M18 14 a5 5 0 0 1 4 6", "people group" },
        { "bell", "Notification", I, "M6 17 V11 a6 6 0 0 1 12 0 V17 L20 19 H4 Z M10 21 H14", "alert bell" },
        { "calendar", "Calendar", I, "M3 6 H21 V21 H3 Z M3 10 H21 M8 3 V7 M16 3 V7", "date schedule" },
        { "clock", "Clock", I, circle + " M12 7 V12 L15.5 14", "time" },
        { "star", "Star", I, "M12 3 L14.6 8.9 L21 9.5 L16.2 13.7 L17.6 20 L12 16.8 L6.4 20 L7.8 13.7 L3 9.5 L9.4 8.9 Z", "favourite" },
        { "heart", "Heart", I, "M12 20 C6 16 3 12.5 3 9 a4.5 4.5 0 0 1 9 -1.5 a4.5 4.5 0 0 1 9 1.5 C21 12.5 18 16 12 20 Z", "love like" },
        { "bookmark", "Bookmark", I, "M6 3 H18 V21 L12 16.5 L6 21 Z", "save" },
        { "flag", "Flag", I, "M5 21 V4 M5 4 H18 L15.5 8.5 L18 13 H5", "milestone" },
        { "lock", "Lock", I, "M5 11 H19 V21 H5 Z M8 11 V7.5 a4 4 0 0 1 8 0 V11", "secure private" },
        { "unlock", "Unlock", I, "M5 11 H19 V21 H5 Z M8 11 V7.5 a4 4 0 0 1 7.8 -1.2", "open" },
        { "eye", "View", I, "M2 12 C5 6.5 8.5 4.5 12 4.5 C15.5 4.5 19 6.5 22 12 C19 17.5 15.5 19.5 12 19.5 C8.5 19.5 5 17.5 2 12 Z M12 9 a3 3 0 1 0 0.01 0", "see visible" },
        { "link", "Link", I, "M10 14 L14 10 M8.5 11.5 L6 14 a3.5 3.5 0 0 0 5 5 L13.5 16.5 M15.5 12.5 L18 10 a3.5 3.5 0 0 0 -5 -5 L10.5 7.5", "url chain" },
        { "trash", "Delete", I, "M4 6 H20 M9 6 V3.5 H15 V6 M6 6 L7 21 H17 L18 6 M10 10 V17 M14 10 V17", "bin remove" },
        { "edit", "Edit", I, "M4 20 L8 19 L19 8 L16 5 L5 16 Z M14 7 L17 10", "pencil write" },
        { "download", "Download", I, "M12 3 V15 M7 10 L12 15 L17 10 M4 19 H20", "save" },
        { "upload", "Upload", I, "M12 15 V3 M7 8 L12 3 L17 8 M4 19 H20", "" },
        { "share", "Share", I, "M18 5 a2.5 2.5 0 1 0 0.01 0 M6 12 a2.5 2.5 0 1 0 0.01 0 M18 19 a2.5 2.5 0 1 0 0.01 0 M8.2 10.8 L15.8 6.2 M8.2 13.2 L15.8 17.8", "network" },
        { "filter", "Filter", I, "M3 5 H21 L14 13 V20 L10 18 V13 Z", "funnel" },
        { "grid", "Grid", I, "M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M14 14 H20 V20 H14 Z", "apps tiles" },
        { "list", "List", I, "M8 6 H20 M8 12 H20 M8 18 H20 M4 6 H4.5 M4 12 H4.5 M4 18 H4.5", "items" },

        { "arrow-up", "Arrow up", R, "M12 20 V4 M6 10 L12 4 L18 10", "" },
        { "arrow-down", "Arrow down", R, "M12 4 V20 M6 14 L12 20 L18 14", "" },
        { "arrow-left", "Arrow left", R, "M20 12 H4 M10 6 L4 12 L10 18", "back" },
        { "arrow-right", "Arrow right", R, "M4 12 H20 M14 6 L20 12 L14 18", "next forward" },
        { "refresh", "Refresh", R, "M20 12 a8 8 0 1 1 -2.3 -5.7 M20 4 V9 H15", "reload sync" },
        { "undo", "Undo", R, "M9 7 L4 12 L9 17 M4 12 H15 a5 5 0 0 1 0 10 H12", "back" },
        { "redo", "Redo", R, "M15 7 L20 12 L15 17 M20 12 H9 a5 5 0 0 0 0 10 H12", "" },
        { "expand", "Expand", R, "M4 9 V4 H9 M15 4 H20 V9 M20 15 V20 H15 M9 20 H4 V15", "fullscreen" },
        { "chevron-right", "Chevron", R, "M9 5 L16 12 L9 19", "next" },
        { "trend-up", "Trend up", R, "M3 17 L9 11 L13 15 L21 7 M15 7 H21 V13", "growth chart" },

        { "mail", "Email", M, "M3 5 H21 V19 H3 Z M3 6 L12 13 L21 6", "message letter" },
        { "chat", "Chat", M, "M4 4 H20 V16 H9 L4 20 Z", "comment message" },
        { "phone", "Phone", M, "M5 3 H9 L11 8 L8.5 9.5 A11 11 0 0 0 14.5 15.5 L16 13 L21 15 V19 A2 2 0 0 1 19 21 A16 16 0 0 1 3 5 A2 2 0 0 1 5 3 Z", "call" },
        { "video", "Video", M, "M3 6 H15 V18 H3 Z M15 10 L21 7 V17 L15 14", "camera meeting" },
        { "send", "Send", M, "M3 11 L21 3 L13 21 L11 13 Z M11 13 L21 3", "paper plane" },
        { "megaphone", "Announcement", M, "M3 10 V14 H7 L15 19 V5 L7 10 Z M18 9 a4 4 0 0 1 0 6", "marketing" },

        { "file", "File", O, "M6 3 H14 L19 8 V21 H6 Z M14 3 V8 H19", "document page" },
        { "folder", "Folder", O, "M3 6 H10 L12 8 H21 V19 H3 Z", "directory" },
        { "image", "Image", O, "M3 5 H21 V19 H3 Z M3 16 L8 11 L12 15 L15 12 L21 17 M15.5 8.5 a1.5 1.5 0 1 0 0.01 0", "picture photo" },
        { "camera", "Camera", O, "M3 8 H7 L9 5 H15 L17 8 H21 V19 H3 Z M12 10 a3.5 3.5 0 1 0 0.01 0", "photo" },
        { "lightbulb", "Idea", O, "M9 18 H15 M10 21 H14 M8.5 14.5 A6 6 0 1 1 15.5 14.5 C14.5 15.5 14.5 16.5 14.5 18 H9.5 C9.5 16.5 9.5 15.5 8.5 14.5 Z", "lightbulb insight" },
        { "target", "Target", O, circle + " M12 7 a5 5 0 1 0 0.01 0 M12 11 a1 1 0 1 0 0.01 0", "goal aim" },
        { "rocket", "Rocket", O, "M12 2 C16 5 17 10 15 15 H9 C7 10 8 5 12 2 Z M9 15 L6 19 L9 18 M15 15 L18 19 L15 18 M12 8 a1.5 1.5 0 1 0 0.01 0", "launch startup" },
        { "gift", "Gift", O, "M3 9 H21 V13 H3 Z M5 13 V21 H19 V13 M12 9 V21 M12 9 C10 5 6 5 7.5 8 M12 9 C14 5 18 5 16.5 8", "present" },
        { "cart", "Cart", O, "M3 4 H6 L8.5 15 H18 L20.5 7 H7 M10 19 a1.3 1.3 0 1 0 0.01 0 M17 19 a1.3 1.3 0 1 0 0.01 0", "shop buy" },
        { "briefcase", "Briefcase", O, "M3 7 H21 V20 H3 Z M9 7 V4 H15 V7 M3 12 H21", "work business" },
        { "book", "Book", O, "M4 4 H10 a2 2 0 0 1 2 2 V20 a2 2 0 0 0 -2 -2 H4 Z M20 4 H14 a2 2 0 0 0 -2 2 V20 a2 2 0 0 1 2 -2 H20 Z", "read docs" },
        { "coffee", "Coffee", O, "M4 8 H16 V15 a4 4 0 0 1 -4 4 H8 a4 4 0 0 1 -4 -4 Z M16 10 H18 a2 2 0 0 1 0 4 H16 M7 3 V5 M10 3 V5 M13 3 V5", "break cup" },
        { "pin", "Location", O, "M12 21 C8 16 5 12.5 5 9 a7 7 0 0 1 14 0 C19 12.5 16 16 12 21 Z M12 6.5 a2.5 2.5 0 1 0 0.01 0", "map place" },
        { "trophy", "Trophy", O, "M7 4 H17 V9 a5 5 0 0 1 -10 0 Z M7 6 H4 a3 3 0 0 0 3 4 M17 6 H20 a3 3 0 0 1 -3 4 M12 14 V18 M8 21 H16 M9 18 H15 V21", "win award" },

        { "info", "Info", S, circle + " M12 11 V16 M12 8 V8.5", "information" },
        { "warning", "Warning", S, "M12 3 L22 20 H2 Z M12 9 V14 M12 17 V17.5", "alert caution" },
        { "help", "Question", S, circle + " M9.5 9.5 a2.5 2.5 0 1 1 3.5 2.3 C12.3 12.2 12 12.8 12 14 M12 17 V17.5", "help faq" },
        { "check-circle", "Success", S, circle + " M8 12.5 L11 15.5 L16.5 9", "done ok" },
        { "x-circle", "Error", S, circle + " M9 9 L15 15 M15 9 L9 15", "fail no" },
        { "smile", "Smile", S, circle + " M8.5 14.5 C10.5 16.5 13.5 16.5 15.5 14.5 M9 10 V10.5 M15 10 V10.5", "happy face" },
        { "thumbs-up", "Thumbs up", S, "M7 11 V21 H4 V11 Z M7 11 L11 3 C12.5 3 13.5 4 13 6 L12.5 9 H19 a2 2 0 0 1 2 2.3 L19.8 19 a2.5 2.5 0 0 1 -2.5 2 H7", "like approve" },
        { "zap", "Lightning", S, "M13 2 L4 14 H11 L10 22 L20 9 H13 Z", "fast energy" },

        { "cloud", "Cloud", T, "M7 19 a4.5 4.5 0 0 1 -0.5 -9 A6 6 0 0 1 18 9 a4 4 0 0 1 -0.5 10 Z", "server hosting" },
        { "database", "Database", T, "M4 6 a8 3 0 1 0 16 0 a8 3 0 1 0 -16 0 M4 6 V18 a8 3 0 0 0 16 0 V6 M4 12 a8 3 0 0 0 16 0", "storage sql" },
        { "server", "Server", T, "M3 4 H21 V10 H3 Z M3 14 H21 V20 H3 Z M7 7 H7.5 M7 17 H7.5", "rack backend" },
        { "code", "Code", T, "M8 7 L3 12 L8 17 M16 7 L21 12 L16 17 M14 4 L10 20", "developer" },
        { "laptop", "Laptop", T, "M5 5 H19 V15 H5 Z M2 19 H22", "computer" },
        { "smartphone", "Phone device", T, "M7 2 H17 V22 H7 Z M11 18.5 H13", "mobile" },
        { "wifi", "Wi-Fi", T, "M2 9 a14 14 0 0 1 20 0 M5.5 12.5 a9 9 0 0 1 13 0 M9 16 a4 4 0 0 1 6 0 M12 19.5 V20", "network internet" },
        { "globe", "Globe", T, circle + " M3 12 H21 M12 3 C9 6 9 18 12 21 M12 3 C15 6 15 18 12 21", "web world" },
        { "cpu", "Chip", T, "M7 7 H17 V17 H7 Z M10 10 H14 V14 H10 Z M9 3 V7 M15 3 V7 M9 17 V21 M15 17 V21 M3 9 H7 M3 15 H7 M17 9 H21 M17 15 H21", "processor hardware" },
    };
}

QStringList categoriesOf(const QList<Entry> &list)
{
    QStringList out;
    for (const Entry &e : list)
        if (!out.contains(e.category))
            out << e.category;
    return out;
}

const Entry *findIn(const QList<Entry> &list, const QString &id)
{
    for (const Entry &e : list)
        if (e.id == id)
            return &e;
    return nullptr;
}

QPainterPath cachedPath(const QString &key, const QString &data)
{
    static QHash<QString, QPainterPath> cache;
    static QMutex mutex;   // exports render on a worker thread
    QMutexLocker lock(&mutex);
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;
    const QPainterPath p = SvgPath::parse(data);
    cache.insert(key, p);
    return p;
}

} // namespace

const QList<Entry> &shapes()
{
    static const QList<Entry> list = buildShapes();
    return list;
}

const QList<Entry> &icons()
{
    static const QList<Entry> list = buildIcons();
    return list;
}

QStringList shapeCategories() { return categoriesOf(shapes()); }
QStringList iconCategories() { return categoriesOf(icons()); }
const Entry *findShape(const QString &id) { return findIn(shapes(), id); }
const Entry *findIcon(const QString &id) { return findIn(icons(), id); }

QPainterPath shapePath(const QString &id, const QRectF &rect)
{
    const Entry *e = findShape(id);
    if (!e)
        return {};
    QTransform t;
    t.translate(rect.x(), rect.y());
    t.scale(rect.width() / 100.0, rect.height() / 100.0);
    return t.map(cachedPath(QStringLiteral("s:") + id, e->path));
}

QPainterPath iconPath(const QString &id, const QRectF &rect)
{
    const Entry *e = findIcon(id);
    if (!e)
        return {};
    const double s = qMin(rect.width(), rect.height()) / 24.0;
    QTransform t;
    t.translate(rect.center().x() - 12 * s, rect.center().y() - 12 * s);
    t.scale(s, s);
    return t.map(cachedPath(QStringLiteral("i:") + id, e->path));
}

const QList<EmojiGroup> &emoji()
{
    static const QList<EmojiGroup> groups = {
        { QStringLiteral("Smileys"), QString::fromUtf8("😀 😃 😄 😁 😆 😅 😂 🙂 😉 😊 😍 🤩 😎 🤔 😐 😴 😢 😭 😡 🤯 🥳 😇 🙃 🤗").split(' ') },
        { QStringLiteral("Gestures"), QString::fromUtf8("👍 👎 👏 🙌 👋 🤝 ✌️ 👌 🙏 💪 ☝️ ✋ 👀 🧠").split(' ') },
        { QStringLiteral("Symbols"), QString::fromUtf8("❤️ 🧡 💛 💚 💙 💜 🖤 ⭐ ✨ 🔥 💯 ✅ ❌ ⚠️ ❓ ❗ 💡 🎯").split(' ') },
        { QStringLiteral("Objects"), QString::fromUtf8("🎉 🎁 🏆 🚀 📌 📎 📅 ⏰ 📝 📣 🔔 💬 📊 📈 🧩 🛠️ 💻 📱").split(' ') },
        { QStringLiteral("Nature"), QString::fromUtf8("🌱 🌸 🌈 ☀️ ⛅ 🌙 🌍 🐶 🐱 🦄 🐝 🍕 ☕ 🍰").split(' ') },
    };
    return groups;
}

} // namespace ShapeLibrary
