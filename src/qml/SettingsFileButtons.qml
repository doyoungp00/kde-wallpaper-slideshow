import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Export and Import buttons for a JSON settings file, and the outcome of the last one.
// target is App (all settings) or a Profile (one tab); both have
// exportSettings(url) and importSettings(url), which return a message or "".
ColumnLayout {
    id: root

    required property var target
    property string exportText: "Export…"

    function report(message, done) {
        result.type = message === "" ? Kirigami.MessageType.Positive : Kirigami.MessageType.Warning
        result.text = message === "" ? done : message
        result.visible = true
    }

    function path(url) {
        return decodeURIComponent(url.toString().replace(/^file:\/\//, ""))
    }

    RowLayout {
        QQC2.Button {
            icon.name: "document-export"
            text: root.exportText
            onClicked: exportDialog.open()
        }
        QQC2.Button {
            icon.name: "document-import"
            text: "Import…"
            onClicked: importDialog.open()
        }
    }

    Kirigami.InlineMessage {
        id: result
        Layout.fillWidth: true
        Layout.preferredWidth: Kirigami.Units.gridUnit * 18
        showCloseButton: true
        visible: false
    }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["Settings files (*.json)"]
        onAccepted: root.report(root.target.exportSettings(selectedFile), "Saved to " + root.path(selectedFile))
    }

    FileDialog {
        id: importDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Settings files (*.json)", "All files (*)"]
        onAccepted: root.report(root.target.importSettings(selectedFile), "Imported " + root.path(selectedFile))
    }
}
