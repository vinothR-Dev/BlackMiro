import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import BlackMiro

Dialog {
    id: dlg

    required property BoardCanvas canvas

    title: "Export board"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, parent ? parent.width - 32 : 640)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : 800)
    padding: 20

    property string compression: "low"
    property var selected: ({ "PNG": true })
    property string resultText: ""
    property bool resultOk: true
    property url resultFolder

    ExportController {
        id: ctl
        canvas: dlg.canvas
        onFinished: (ok, message, files, folder) => {
            dlg.resultOk = ok
            dlg.resultText = message
            dlg.resultFolder = folder
        }
    }

    function selectedNames() {
        let out = []
        for (const f of ctl.formats)
            if (f.available && dlg.selected[f.name])
                out.push(f.name)
        return out
    }

    function setAll(on) {
        let s = {}
        for (const f of ctl.formats)
            s[f.name] = on && f.available
        dlg.selected = s
        zipBox.checked = on
    }

    onOpened: {
        resultText = ""
        exportPassword.text = ""
        if (fileName.text.length === 0)
            fileName.text = canvas.boardTitle.replace(/[\\/:*?"<>|]+/g, "-")
        scopeBox.currentIndex = canvas.selectionCount > 0 ? 1 : 0
    }

    // Android: the system "create document" picker; the export is written to the chosen URI.
    FileDialog {
        id: saveAsDialog
        title: "Save export as"
        fileMode: FileDialog.SaveFile
        onAccepted: ctl.startToUrl(dlg.pendingOptions, selectedFile)
    }
    property var pendingOptions: ({})

    function currentOptions() {
        return {
            formats: dlg.selectedNames(),
            compression: dlg.compression,
            scope: ["board", "selection", "frames"][scopeBox.currentIndex],
            folder: folderField.text,
            baseName: fileName.text,
            transparent: transparentBox.checked,
            zip: zipBox.checked,
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
        title: "Choose export folder"
        onAccepted: folderField.text = ctl.toLocalPath(selectedFolder)
    }

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 14

            // ---- Formats
            Label { text: "File types"; font.bold: true }
            CheckBox {
                id: allBox
                text: dlg.width < 560 ? "All file types (one ZIP)" : "All file types (bundled in one ZIP, one folder per format)"
                checked: dlg.selectedNames().length === ctl.formats.filter(f => f.available).length
                onClicked: dlg.setAll(checked)
            }
            GridLayout {
                columns: dlg.width < 440 ? 2 : dlg.width < 560 ? 3 : 4
                columnSpacing: 8
                rowSpacing: 0
                Repeater {
                    model: ctl.formats
                    delegate: CheckBox {
                        required property var modelData
                        text: modelData.name + (modelData.available ? "" : " (n/a)")
                        enabled: modelData.available
                        checked: !!dlg.selected[modelData.name] && modelData.available
                        onClicked: {
                            let s = Object.assign({}, dlg.selected)
                            s[modelData.name] = checked
                            dlg.selected = s
                        }
                        ToolTip.visible: hovered && !modelData.available
                        ToolTip.text: "No encoder plugin for this format in this build"
                    }
                }
            }

            // ---- Compression
            Label { text: "Compression"; font.bold: true }
            RowLayout {
                spacing: 4
                Repeater {
                    model: [
                        { key: "low", label: "Low · Lossless" },
                        { key: "medium", label: "Medium · Average" },
                        { key: "high", label: "High · Small" },
                        { key: "custom", label: "Custom" }
                    ]
                    delegate: Button {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        checked: dlg.compression === modelData.key
                        onClicked: dlg.compression = modelData.key
                        focusPolicy: Qt.NoFocus
                        background: Rectangle {
                            radius: 8
                            color: parent.checked ? Theme.accent : parent.hovered ? Theme.hover : Theme.surfaceAlt
                        }
                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? Theme.accentText : Theme.text
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.subText
                text: ctl.describe(dlg.compression, dpiBox.value, qualitySlider.value, bitBox.checked)
            }
            GridLayout {
                visible: dlg.compression === "custom"
                columns: 3
                Label { text: "Resolution (DPI)" }
                SpinBox { id: dpiBox; from: 72; to: 2400; stepSize: 50; value: 300; editable: true }
                Item { Layout.fillWidth: true }
                Label { text: "Quality" }
                Slider { id: qualitySlider; from: 1; to: 100; stepSize: 1; value: 90; Layout.fillWidth: true }
                Label { text: qualitySlider.value >= 100 ? "100 (lossless)" : Math.round(qualitySlider.value) }
                Label { text: "Colour depth" }
                CheckBox { id: bitBox; text: "16 bits per channel (PNG / TIFF)" }
                Item {}
            }

            // ---- Scope & options
            Label { text: "What to export"; font.bold: true }
            ComboBox {
                id: scopeBox
                Layout.fillWidth: true
                model: ["Whole board", "Selection only", "Each frame separately (PDF pages / one file per frame)"]
            }
            CheckBox { id: transparentBox; text: "Transparent background (PNG, TIFF, WebP, SVG, PDF)" }
            CheckBox { id: zipBox; text: "Bundle into one ZIP file" }

            // ---- Private board
            Rectangle {
                visible: dlg.canvas.boardPrivate
                Layout.fillWidth: true
                implicitHeight: privCol.implicitHeight + 20
                radius: 8
                color: Theme.accentSoft
                ColumnLayout {
                    id: privCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: "Private board — enter the board password to export"
                    }
                    TextField {
                        id: exportPassword
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: "Board password"
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.subText
                        text: "PDF and BOARD files are encrypted with this password. Images, SVG, CSV, Markdown "
                              + "and JSON can't carry a password, so they are exported unprotected."
                    }
                }
            }

            // ---- Destination
            Label { text: "Save to"; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: fileName
                    placeholderText: "File name"
                    Layout.preferredWidth: 180
                    Layout.fillWidth: ctl.singleFileExport
                }
                TextField {
                    id: folderField
                    visible: !ctl.singleFileExport
                    Layout.fillWidth: true
                    placeholderText: "Documents folder"
                }
                Button { visible: !ctl.singleFileExport; text: "Browse…"; onClicked: folderDialog.open() }
            }
            Label {
                visible: ctl.singleFileExport
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.subText
                text: (Qt.platform.os === "wasm" ? "Downloads as " : "Saves as ")
                      + ctl.packageName(dlg.currentOptions())
                      + " (several files are bundled into one ZIP)."
            }

            // ---- Result
            RowLayout {
                Layout.fillWidth: true
                visible: ctl.busy || dlg.resultText.length > 0
                BusyIndicator { running: ctl.busy; visible: ctl.busy; implicitWidth: 32; implicitHeight: 32 }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: ctl.busy ? "Rendering…" : dlg.resultText
                    color: dlg.resultOk ? Theme.success : Theme.danger
                }
                Button {
                    visible: !ctl.busy && dlg.resultOk && dlg.resultText.length > 0 && !ctl.singleFileExport
                    text: "Open folder"
                    onClicked: Qt.openUrlExternally(dlg.resultFolder)
                }
            }
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight
        Button {
            text: "Close"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            id: exportGo
            text: ctl.busy ? "Exporting…" : "Export"
            enabled: !ctl.busy && dlg.selectedNames().length > 0
                     && (!dlg.canvas.boardPrivate || exportPassword.text.length > 0)
            highlighted: true
            onClicked: dlg.runExport()
        }
    }
}
