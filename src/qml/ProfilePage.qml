import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Slideshow

// Everything one profile shows: its folders, timing, scaling, video, playlist and
// edge controls. Each control writes its setting directly.
QQC2.ScrollView {
    id: page

    required property Profile profile
    readonly property ProfileSettings s: profile.settings

    contentWidth: availableWidth

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            type: Kirigami.MessageType.Error
            text: page.profile.folderError
            visible: page.profile.folderError !== ""
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Library"
            }

            ColumnLayout {
                Kirigami.FormData.label: "Folders:"
                Kirigami.FormData.labelAlignment: Qt.AlignTop
                Layout.fillWidth: true

                Repeater {
                    model: page.s.folders
                    RowLayout {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 12
                            text: modelData
                            elide: Text.ElideMiddle
                        }
                        QQC2.ToolButton {
                            icon.name: "list-remove"
                            onClicked: page.profile.removeFolder(index)
                            QQC2.ToolTip.text: "Remove this folder"
                            QQC2.ToolTip.visible: hovered
                        }
                    }
                }

                QQC2.Label {
                    text: "No folders yet"
                    visible: page.s.folders.length === 0
                    opacity: 0.7
                }

                QQC2.Button {
                    icon.name: "list-add"
                    text: "Add Folder…"
                    onClicked: folderDialog.open()
                }
            }

            QQC2.CheckBox {
                text: "Include subfolders"
                checked: page.s.includeSubfolders
                onToggled: page.s.includeSubfolders = checked
            }

            RowLayout {
                QQC2.Button {
                    icon.name: "view-refresh"
                    text: "Rescan"
                    onClicked: page.profile.rescan()
                }
                QQC2.Button {
                    icon.name: "media-playlist-shuffle"
                    text: "Reshuffle"
                    onClicked: page.profile.reshuffle()
                }
                QQC2.BusyIndicator {
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    Layout.preferredWidth: Layout.preferredHeight
                    running: page.profile.scanning
                    visible: running
                }
                QQC2.Label {
                    text: page.profile.scanning ? "Scanning…" : page.profile.libraryCount + " files"
                }
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Playback"
            }

            RowLayout {
                Kirigami.FormData.label: "Show each item for:"
                QQC2.SpinBox {
                    from: 1
                    to: 86400
                    editable: true
                    value: page.s.durationSec
                    onValueModified: page.s.durationSec = value
                }
                QQC2.Label { text: "seconds" }
            }

            QQC2.CheckBox {
                text: "Freeze animations and videos while paused"
                checked: page.s.freezeAnimationsOnPause
                onToggled: page.s.freezeAnimationsOnPause = checked
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Display"
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: "Scaling:"
                model: ["Cover", "Fit", "Stretch"]
                currentIndex: page.s.scaleMode
                onActivated: index => page.s.scaleMode = index
            }

            ColorRow {
                Kirigami.FormData.label: "Letterbox color:"
                enabled: page.s.scaleMode === 1
                key: "letterboxColor"
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Smart duration"
            }

            QQC2.CheckBox {
                text: "Show animations and videos for one full loop"
                checked: page.s.smartDuration
                onToggled: page.s.smartDuration = checked
            }

            QQC2.CheckBox {
                text: "Round short loops up to whole loops"
                enabled: page.s.smartDuration
                checked: page.s.roundUpToLoops
                onToggled: page.s.roundUpToLoops = checked
            }

            RowLayout {
                Kirigami.FormData.label: "Maximum duration:"
                enabled: page.s.smartDuration
                QQC2.SpinBox {
                    from: 1
                    to: 86400
                    editable: true
                    value: page.s.maxDurationSec
                    onValueModified: page.s.maxDurationSec = value
                }
                QQC2.Label { text: "seconds" }
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Video"
            }

            QQC2.CheckBox {
                text: "Play MP4 and WebM videos"
                checked: page.s.videoEnabled
                onToggled: page.s.videoEnabled = checked
            }

            QQC2.CheckBox {
                text: "Mute"
                enabled: page.s.videoEnabled
                checked: page.s.mute
                onToggled: page.s.mute = checked
            }

            QQC2.Slider {
                Kirigami.FormData.label: "Volume:"
                enabled: page.s.videoEnabled && !page.s.mute
                from: 0
                to: 100
                stepSize: 1
                value: page.s.volume
                onMoved: page.s.volume = value
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Playlist"
            }

            QQC2.CheckBox {
                text: "Show the playlist"
                checked: page.s.playlistVisible
                onToggled: page.s.playlistVisible = checked
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: "Corner:"
                model: ["Top left", "Top right", "Bottom left", "Bottom right"]
                currentIndex: page.s.playlistCorner
                onActivated: index => page.s.playlistCorner = index
            }

            RowLayout {
                Kirigami.FormData.label: "Size:"
                QQC2.SpinBox {
                    from: 160
                    to: 4000
                    stepSize: 10
                    editable: true
                    value: page.s.playlistWidth
                    onValueModified: page.s.playlistWidth = value
                }
                QQC2.Label { text: "×" }
                QQC2.SpinBox {
                    from: 120
                    to: 4000
                    stepSize: 10
                    editable: true
                    value: page.s.playlistHeight
                    onValueModified: page.s.playlistHeight = value
                }
            }

            RowLayout {
                Kirigami.FormData.label: "Margins:"
                QQC2.SpinBox {
                    from: 0
                    to: 2000
                    editable: true
                    value: page.s.playlistMarginX
                    onValueModified: page.s.playlistMarginX = value
                }
                QQC2.Label { text: "×" }
                QQC2.SpinBox {
                    from: 0
                    to: 2000
                    editable: true
                    value: page.s.playlistMarginY
                    onValueModified: page.s.playlistMarginY = value
                }
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: "Colors:"
                model: ["System", "Light", "Dark"]
                currentIndex: page.s.playlistTheme
                onActivated: index => page.s.playlistTheme = index
            }

            QQC2.Slider {
                Kirigami.FormData.label: "Opacity:"
                from: 0.1
                to: 1
                value: page.s.playlistOpacity
                onMoved: page.s.playlistOpacity = value
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Filename"
            }

            QQC2.CheckBox {
                text: "Show the current filename in the top right corner"
                checked: page.s.titleVisible
                onToggled: page.s.titleVisible = checked
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: "Show:"
                enabled: page.s.titleVisible
                model: ["Always", "When the pointer is near"]
                currentIndex: page.s.titleShowOnHover ? 1 : 0
                onActivated: index => page.s.titleShowOnHover = index === 1
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: "Font:"
                enabled: page.s.titleVisible
                model: ["System font"].concat(Qt.fontFamilies())
                currentIndex: page.s.titleFontFamily === "" ? 0 : Math.max(0, model.indexOf(page.s.titleFontFamily))
                onActivated: index => page.s.titleFontFamily = index === 0 ? "" : model[index]
            }

            RowLayout {
                Kirigami.FormData.label: "Margins:"
                enabled: page.s.titleVisible
                QQC2.Label { text: "Top" }
                QQC2.SpinBox {
                    from: 0
                    to: 2000
                    editable: true
                    value: page.s.titleMarginTop
                    onValueModified: page.s.titleMarginTop = value
                }
                QQC2.Label { text: "Right" }
                QQC2.SpinBox {
                    from: 0
                    to: 2000
                    editable: true
                    value: page.s.titleMarginRight
                    onValueModified: page.s.titleMarginRight = value
                }
            }

            RowLayout {
                Kirigami.FormData.label: "Text size:"
                enabled: page.s.titleVisible
                QQC2.SpinBox {
                    from: 6
                    to: 400
                    editable: true
                    value: page.s.titleFontSize
                    onValueModified: page.s.titleFontSize = value
                }
                QQC2.Label { text: "px" }
            }

            ColorRow {
                Kirigami.FormData.label: "Fill color:"
                enabled: page.s.titleVisible
                key: "titleColor"
            }

            RowLayout {
                Kirigami.FormData.label: "Border:"
                enabled: page.s.titleVisible
                QQC2.CheckBox {
                    checked: page.s.titleOutline
                    onToggled: page.s.titleOutline = checked
                }
                ColorRow {
                    enabled: page.s.titleOutline
                    key: "titleOutlineColor"
                }
            }

            QQC2.CheckBox {
                text: "Drop shadow"
                enabled: page.s.titleVisible
                checked: page.s.titleShadow
                onToggled: page.s.titleShadow = checked
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Edge controls"
            }

            RowLayout {
                Kirigami.FormData.label: "Edges:"
                QQC2.CheckBox { text: "Top"; checked: page.s.edgeTop; onToggled: page.s.edgeTop = checked }
                QQC2.CheckBox { text: "Bottom"; checked: page.s.edgeBottom; onToggled: page.s.edgeBottom = checked }
                QQC2.CheckBox { text: "Left"; checked: page.s.edgeLeft; onToggled: page.s.edgeLeft = checked }
                QQC2.CheckBox { text: "Right"; checked: page.s.edgeRight; onToggled: page.s.edgeRight = checked }
            }

            RowLayout {
                Kirigami.FormData.label: "Hover strip width:"
                QQC2.SpinBox {
                    from: 2
                    to: 64
                    value: page.s.edgeStripWidth
                    onValueModified: page.s.edgeStripWidth = value
                }
                QQC2.Label { text: "px" }
            }

            RowLayout {
                Kirigami.FormData.label: "Hide after:"
                QQC2.SpinBox {
                    from: 0
                    to: 10000
                    stepSize: 100
                    editable: true
                    value: page.s.edgeHideDelayMs
                    onValueModified: page.s.edgeHideDelayMs = value
                }
                QQC2.Label { text: "ms" }
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: "Settings File"
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                wrapMode: Text.WordWrap
                text: page.profile.id === "default"
                      ? "Save this tab's settings to a file, or apply a settings file to all screens."
                      : "Save this screen's settings to a file, or apply a settings file to this screen only."
            }

            SettingsFileButtons {
                target: page.profile
            }
        }
    }

    FolderDialog {
        id: folderDialog
        onAccepted: page.profile.addFolder(selectedFolder)
    }

    // A swatch of the color setting named key and a button that changes it.
    component ColorRow: RowLayout {
        id: colorRow
        required property string key
        Rectangle {
            implicitWidth: Kirigami.Units.gridUnit * 2
            implicitHeight: Kirigami.Units.gridUnit
            color: page.s[colorRow.key]
            border.color: Kirigami.Theme.textColor
            radius: 3
        }
        QQC2.Button {
            text: "Choose…"
            onClicked: {
                colorDialog.key = colorRow.key
                colorDialog.selectedColor = page.s[colorRow.key]
                colorDialog.open()
            }
        }
    }

    ColorDialog {
        id: colorDialog
        property string key
        onAccepted: page.s[key] = selectedColor
    }
}
