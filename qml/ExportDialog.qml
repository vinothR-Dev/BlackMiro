import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Miru

// Export in three choices: format (PDF by default), quality, and what to export.
// Everything else sits under "More options".
Dialog {
    id: dlg

    required property BoardCanvas canvas

    title: qsTr("Export")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(600, parent ? parent.width - 32 : 600)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : 760)
    padding: 18

    // One format, or every format at once ("ALL").
    property string format: "PDF"
    property string compression: "low"
    property int scope: 0                     // 0 board, 1 selection, 2 each frame
    property bool advanced: false
    property string resultText: ""
    property bool resultOk: true
    property url resultFolder

    readonly property var mainFormats: [
        { name: "PDF", label: qsTr("PDF"), hint: qsTr("Document, vector") },
        { name: "PNG", label: qsTr("PNG"), hint: qsTr("Image, lossless") },
        { name: "SVG", label: qsTr("SVG"), hint: qsTr("Vector graphic") },
        { name: "JPEG", label: qsTr("JPEG"), hint: qsTr("Photo, small") },
        { name: "WEBP", label: qsTr("WebP"), hint: qsTr("Web image") },
        { name: "TIFF", label: qsTr("TIFF"), hint: qsTr("Print image") },
        { name: "BOARD", label: qsTr("Miru board"), hint: qsTr("Editable file") },
        { name: "ALL", label: qsTr("All formats"), hint: qsTr("One ZIP") }
    ]
    readonly property var otherFormats: ["CSV", "MARKDOWN", "JSON", "AVIF"]

    ExportController {
        id: ctl
        canvas: dlg.canvas
        onFinished: (ok, message, files, folder) => {
            dlg.resultOk = ok
            dlg.resultText = message
            dlg.resultFolder = folder
        }
    }

    function available(name) {
        if (name === "ALL")
            return true
        for (const f of ctl.formats)
            if (f.name === name)
                return f.available
        return false
    }
    function selectedNames() {
        if (dlg.format !== "ALL")
            return [dlg.format]
        return ctl.formats.filter(f => f.available).map(f => f.name)
    }

    onOpened: {
        resultText = ""
        exportPassword.text = ""
        if (fileName.text.length === 0)
            fileName.text = canvas.boardTitle.replace(/[\\/:*?"<>|]+/g, "-")
        scope = canvas.selectionCount > 0 ? 1 : 0
    }

    // Android: the system "create document" picker; the export is written to the chosen URI.
    FileDialog {
        id: saveAsDialog
        title: qsTr("Save export as")
        fileMode: FileDialog.SaveFile
        onAccepted: ctl.startToUrl(dlg.pendingOptions, selectedFile)
    }
    property var pendingOptions: ({})

    function currentOptions() {
        return {
            formats: dlg.selectedNames(),
            compression: dlg.compression,
            scope: ["board", "selection", "frames"][dlg.scope],
            folder: folderField.text,
            baseName: fileName.text,
            transparent: transparentBox.checked,
            zip: dlg.format === "ALL" || zipBox.checked,
            customDpi: dpiBox.value,
            customQuality: Math.round(qualitySlider.value),
            custom16Bit: bitBox.checked,
            password: exportPassword.text
        }
    }

    function runExport() {
        const opts = currentOptions()
        if (Qt.platform.os === "android") {
            pendingOptions = opts
            const name = ctl.packageName(opts)
            const ext = name.substring(name.lastIndexOf(".") + 1)
            saveAsDialog.nameFilters = [ext.toUpperCase() + " file (*." + ext + ")"]
            saveAsDialog.selectedFile = name
            saveAsDialog.open()
        } else {
            ctl.start(opts)
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose export folder")
        onAccepted: folderField.text = ctl.toLocalPath(selectedFolder)
    }

    // Selectable card (format / quality / scope): name with a short hint below.
    component Choice: AbstractButton {
        id: choice
        property string hint
        property bool current
        implicitHeight: hint.length > 0 ? 46 : 36
        Layout.preferredWidth: 1          // equal widths inside a GridLayout row
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.RadioButton
        Accessible.checked: current
        Accessible.description: hint
        background: Rectangle {
            radius: 9
            color: choice.current ? Theme.accentSoft : choice.hovered ? Theme.hover : Theme.surfaceAlt
            border.width: choice.current ? 2 : 1
            border.color: choice.current || choice.activeFocus ? Theme.accent : Theme.border
        }
        contentItem: Column {
            spacing: 0
            leftPadding: 10
            topPadding: (choice.height - implicitHeight) / 2
            Label {
                text: choice.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: choice.enabled ? (choice.current ? Theme.accent : Theme.text) : Theme.iconDisabled
            }
            Label {
                text: choice.hint
                font.pixelSize: 11
                color: choice.enabled ? Theme.subText : Theme.iconDisabled
                visible: text.length > 0
            }
        }
    }

    component Heading: Label {
        font.pixelSize: 13
        font.weight: Font.DemiBold
        color: Theme.text
        Layout.topMargin: 2
    }

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 8

            // ---- 1. Format
            Heading { text: qsTr("Format") }
            GridLayout {
                Layout.fillWidth: true
                columns: dlg.width < 440 ? 2 : 4
                columnSpacing: 6
                rowSpacing: 6
                Repeater {
                    model: dlg.mainFormats
                    delegate: Choice {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.label
                        hint: dlg.available(modelData.name) ? modelData.hint : qsTr("Not available here")
                        enabled: dlg.available(modelData.name)
                        current: dlg.format === modelData.name
                        onClicked: dlg.format = modelData.name
                    }
                }
            }
            RowLayout {
                spacing: 6
                Label { text: qsTr("Data:"); color: Theme.subText }
                Repeater {
                    model: dlg.otherFormats
                    delegate: Button {
                        required property string modelData
                        flat: true
                        visible: dlg.available(modelData)
                        text: modelData === "MARKDOWN" ? "Markdown" : modelData
                        checkable: true
                        checked: dlg.format === modelData
                        onClicked: dlg.format = modelData
                    }
                }
            }

            // ---- 2. Quality
            Heading { text: qsTr("Quality") }
            GridLayout {
                Layout.fillWidth: true
                columns: dlg.width < 440 ? 1 : 3
                columnSpacing: 6
                rowSpacing: 6
                Choice {
                    Layout.fillWidth: true
                    text: qsTr("Best · lossless")
                    hint: qsTr("Sharpest, largest file")
                    current: dlg.compression === "low"
                    onClicked: dlg.compression = "low"
                }
                Choice {
                    Layout.fillWidth: true
                    text: qsTr("Balanced")
                    hint: qsTr("Good quality, medium file")
                    current: dlg.compression === "medium"
                    onClicked: dlg.compression = "medium"
                }
                Choice {
                    Layout.fillWidth: true
                    text: qsTr("Smallest")
                    hint: qsTr("For email and chat")
                    current: dlg.compression === "high"
                    onClicked: dlg.compression = "high"
                }
            }

            // ---- 3. What to export
            Heading { text: qsTr("What to export") }
            GridLayout {
                Layout.fillWidth: true
                columns: dlg.width < 440 ? 1 : 3
                columnSpacing: 6
                rowSpacing: 6
                Choice {
                    Layout.fillWidth: true
                    text: qsTr("Whole board")
                    current: dlg.scope === 0
                    onClicked: dlg.scope = 0
                }
                Choice {
                    Layout.fillWidth: true
                    text: qsTr("Selection")
                    hint: dlg.canvas.selectionCount > 0 ? qsTr("%1 selected").arg(dlg.canvas.selectionCount) : qsTr("Nothing selected")
                    enabled: dlg.canvas.selectionCount > 0
                    current: dlg.scope === 1
                    onClicked: dlg.scope = 1
                }
                Choice {
                    Layout.fillWidth: true
                    text: qsTr("Each frame")
                    hint: qsTr("One page or file per frame")
                    current: dlg.scope === 2
                    onClicked: dlg.scope = 2
                }
            }

            // ---- Private board
            Rectangle {
                visible: dlg.canvas.boardPrivate
                Layout.fillWidth: true
                implicitHeight: privCol.implicitHeight + 24
                radius: 12
                color: Theme.accentSoft
                ColumnLayout {
                    id: privCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.weight: Font.DemiBold
                        color: Theme.text
                        text: qsTr("Private board: enter its password to export")
                    }
                    TextField {
                        id: exportPassword
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: qsTr("Board password")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.subText
                        font.pixelSize: 12
                        text: qsTr("PDF and Miru board files are encrypted with this password. Images, SVG and data files can't carry a password.")
                    }
                }
            }

            // ---- More options
            Button {
                flat: true
                text: (dlg.advanced ? "▾ " : "▸ ") + qsTr("More options")
                onClicked: dlg.advanced = !dlg.advanced
                Layout.leftMargin: -8
            }
            ColumnLayout {
                visible: dlg.advanced
                Layout.fillWidth: true
                spacing: 8
                CheckBox { id: transparentBox; text: qsTr("Transparent background (PNG, TIFF, WebP, SVG, PDF)") }
                CheckBox { id: zipBox; visible: dlg.format !== "ALL"; text: qsTr("Bundle into one ZIP file") }
                CheckBox {
                    id: customBox
                    text: qsTr("Custom quality (DPI, image quality, colour depth)")
                    checked: dlg.compression === "custom"
                    onClicked: dlg.compression = checked ? "custom" : "low"
                }
                GridLayout {
                    visible: dlg.compression === "custom"
                    Layout.leftMargin: 28
                    columns: 3
                    Label { text: qsTr("Resolution (DPI)"); color: Theme.text }
                    SpinBox { id: dpiBox; from: 72; to: 2400; stepSize: 50; value: 300; editable: true }
                    Item { Layout.fillWidth: true }
                    Label { text: qsTr("Image quality"); color: Theme.text }
                    Slider { id: qualitySlider; from: 1; to: 100; stepSize: 1; value: 90; Layout.fillWidth: true }
                    Label { text: qualitySlider.value >= 100 ? qsTr("100 (lossless)") : Math.round(qualitySlider.value); color: Theme.text }
                    Label { text: qsTr("Colour depth"); color: Theme.text }
                    CheckBox { id: bitBox; text: qsTr("16 bits per channel (PNG / TIFF)") }
                    Item {}
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.subText
                    font.pixelSize: 12
                    text: ctl.describe(dlg.compression, dpiBox.value, qualitySlider.value, bitBox.checked)
                }
                RowLayout {
                    visible: !ctl.singleFileExport
                    Layout.fillWidth: true
                    TextField {
                        id: folderField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Folder: Documents")
                    }
                    Button { text: qsTr("Browse…"); onClicked: folderDialog.open() }
                }
            }

            // ---- Result
            RowLayout {
                Layout.fillWidth: true
                visible: ctl.busy || dlg.resultText.length > 0
                BusyIndicator { running: ctl.busy; visible: ctl.busy; implicitWidth: 32; implicitHeight: 32 }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: ctl.busy ? qsTr("Rendering…") : dlg.resultText
                    color: dlg.resultOk ? Theme.success : Theme.danger
                }
                Button {
                    visible: !ctl.busy && dlg.resultOk && dlg.resultText.length > 0 && !ctl.singleFileExport
                    text: qsTr("Open folder")
                    onClicked: Qt.openUrlExternally(dlg.resultFolder)
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footRow.implicitHeight + 24
        RowLayout {
            id: footRow
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 18; rightMargin: 18 }
            spacing: 10
            TextField {
                id: fileName
                Layout.fillWidth: true
                Layout.maximumWidth: 240
                placeholderText: qsTr("File name")
                Accessible.name: qsTr("File name")
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                color: Theme.subText
                font.pixelSize: 12
                text: ctl.packageName(dlg.currentOptions())
            }
            Button { text: qsTr("Cancel"); onClicked: dlg.close() }
            Button {
                text: ctl.busy ? qsTr("Exporting…") : qsTr("Export")
                highlighted: true
                enabled: !ctl.busy && dlg.selectedNames().length > 0
                         && (!dlg.canvas.boardPrivate || exportPassword.text.length > 0)
                onClicked: dlg.runExport()
            }
        }
    }
}
