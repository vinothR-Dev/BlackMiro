import QtQuick
import QtQuick.Shapes
import Miru

// The Miru mark (vector, from assets/miru-logo.svg). Grey on light themes,
// light grey on dark themes so it keeps its contrast.
Item {
    id: logo

    implicitWidth: 32
    implicitHeight: 32
    Accessible.role: Accessible.Graphic
    Accessible.name: "Miru"

    readonly property color colorTop: Theme.dark ? "#f2f3f5" : "#9a9a9a"
    readonly property color colorBottom: Theme.dark ? "#9ba1ad" : "#2c2c2c"

    Shape {
        anchors.centerIn: parent
        width: 100
        height: 100
        scale: Math.min(logo.width, logo.height) / 100
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: 44; x2: 0; y2: 93
                GradientStop { position: 0; color: Qt.darker(logo.colorTop, Theme.dark ? 1.0 : 1.1) }
                GradientStop { position: 1; color: logo.colorBottom }
            }
            PathSvg {
                path: "M1.7 57.1 C17.3 55.4 25.6 44.7 32.3 46.7 C40.6 49.2 38.6 60.2 35.6 66.9 C31.4 76.5 33.6 88.8 42.4 88.8 C52.4 88.8 56.8 78.6 55.5 68.8 C54.4 60.2 53.8 49.1 61.8 46.7 C69.2 44.6 81.3 55.2 98.3 57.4 C81 57.6 68.8 48.8 62.4 50.9 C58.6 52.6 58.8 60 59.8 68.2 C61.2 79.2 56 93 42.4 93 C30.6 93 26 77.8 31.4 65.3 C34 59.6 36 52.6 31.9 50.9 C26.2 48.8 17.3 57.4 1.7 57.1 Z"
            }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: 8; x2: 0; y2: 86
                GradientStop { position: 0; color: logo.colorTop }
                GradientStop { position: 1; color: logo.colorBottom }
            }
            PathSvg {
                path: "M44.2 8 C46.5 24 51.5 44 52.4 62 C53.3 77 49.5 86.4 43.6 86.4 C38.6 86.4 36.6 81.6 37.6 76.6 C38.8 70.6 44.2 66.4 45.4 56 C46.3 44 45.6 24 44.2 8 Z"
            }
        }
    }
}
