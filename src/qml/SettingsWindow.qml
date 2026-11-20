import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Slideshow

// A General tab for app-wide settings, then one tab per profile: a single "All
// screens" tab, or one per screen when screens have separate settings. Every
// control writes its setting directly; closing the window hides it to the tray.
Kirigami.ApplicationWindow {
    id: root

    readonly property var s: App.settings
    // The trayClick and trayDoubleClick choices; value is the index in settings.kcfg.
    readonly property var trayActions: [
        {value: 0, text: "Nothing", icon: "action-unavailable-symbolic"},
        {value: 1, text: "Open or close settings", icon: "configure"},
        {value: 2, text: "Play", icon: "media-playback-start"},
        {value: 3, text: "Pause", icon: "media-playback-pause"},
        {value: 15, text: "Play or pause", icon: "media-playback-playing"},
        {value: 4, text: "Previous", icon: "media-skip-backward"},
        {value: 5, text: "Next", icon: "media-skip-forward"},
        {value: 6, text: "Go to start of playlist", icon: "go-first"},
        {value: 7, text: "Go to end of playlist", icon: "go-last"},
        {value: 8, text: "Shuffle", icon: "media-playlist-shuffle"},
        {value: 9, text: "Rescan folders", icon: "view-refresh"},
        {value: 10, text: "Show or hide", icon: "visibility"},
        {value: 11, text: "Show", icon: "view-visible"},
        {value: 12, text: "Hide", icon: "view-hidden"},
        {value: 13, text: "Mute or unmute", icon: "audio-volume-muted"},
        {value: 14, text: "Prevent lock and sleep, or allow them", icon: "system-suspend-inhibited"}
    ]

    // A check box indented one level, for an option that refines the one above it.
    component SubCheckBox: QQC2.CheckBox {
        leftPadding: padding + (mirrored ? 0 : Kirigami.Units.gridUnit)
        rightPadding: padding + (mirrored ? Kirigami.Units.gridUnit : 0)
    }

    // A tray action picker: each choice with its icon, in the list and the box.
    // value is the setting; picked(value) reports a choice. The position is looked up
    // here rather than with indexOfValue(), which gives -1 until the list has loaded
    // and isn't evaluated again after that.
    component TrayActionBox: QQC2.ComboBox {
        id: box
        property int value
        signal picked(int value)
        model: root.trayActions
        textRole: "text"
        currentIndex: root.trayActions.findIndex(action => action.value === value)
        onActivated: index => picked(root.trayActions[index].value)
        Kirigami.StyleHints.iconName: currentIndex >= 0 ? root.trayActions[currentIndex].icon : ""
        delegate: QQC2.MenuItem {
            required property var modelData
            required property int index
            text: modelData.text
            icon.name: modelData.icon
            highlighted: box.highlightedIndex === index
        }
    }

    title: "Wallpaper Slideshow"
    width: Kirigami.Units.gridUnit * 34
    height: Kirigami.Units.gridUnit * 44
    minimumWidth: Kirigami.Units.gridUnit * 26
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: false

    onClosing: close => {
        close.accepted = false
        root.hide()
    }

    pageStack.initialPage: Kirigami.Page {
        title: "Settings"
        padding: 0

        actions: [
            Kirigami.Action {
                text: App.running ? "Stop" : "Start"
                icon.name: App.running ? "media-playback-stop" : "media-playback-start"
                onTriggered: App.running = !App.running
            },
            Kirigami.Action {
                text: App.controller && App.controller.paused ? "Play" : "Pause"
                icon.name: App.controller && App.controller.paused ? "media-playback-start" : "media-playback-pause"
                enabled: App.running
                onTriggered: App.togglePause()
            }
        ]

        header: QQC2.TabBar {
            id: tabs

            QQC2.TabButton {
                text: "General"
                width: implicitWidth
            }
            Repeater {
                model: App.profiles
                QQC2.TabButton {
                    required property Profile modelData
                    text: modelData.title
                    width: implicitWidth
                }
            }
        }

        StackLayout {
            anchors.fill: parent
            currentIndex: tabs.currentIndex

            QQC2.ScrollView {
                id: general
                contentWidth: availableWidth

                ColumnLayout {
                    width: general.availableWidth
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        Layout.margins: Kirigami.Units.largeSpacing
                        type: Kirigami.MessageType.Warning
                        text: "This system's Qt image plugins can't play animated WebP files; they will show as still images. Install qt6-qtimageformats built with libwebp."
                        visible: !App.animatedWebpSupported
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true

                        Kirigami.Separator {
                            Kirigami.FormData.isSection: true
                            Kirigami.FormData.label: "Screens"
                        }

                        QQC2.CheckBox {
                            text: "Use separate settings for each screen"
                            checked: root.s.separateScreens
                            onToggled: root.s.separateScreens = checked
                        }

                        FormColumn {
                            Kirigami.FormData.label: "Show on:"
                            Kirigami.FormData.labelAlignment: Qt.AlignTop
                            Repeater {
                                model: App.screenNames
                                QQC2.CheckBox {
                                    required property string modelData
                                    text: modelData
                                    checked: root.s.disabledScreens.indexOf(modelData) < 0
                                    onToggled: App.setScreenEnabled(modelData, checked)
                                }
                            }
                        }

                        Kirigami.Separator {
                            Kirigami.FormData.isSection: true
                            Kirigami.FormData.label: "General"
                        }

                        QQC2.CheckBox {
                            text: "Start at login"
                            checked: root.s.autostart
                            onToggled: root.s.autostart = checked
                        }

                        QQC2.CheckBox {
                            text: "Watch folders for changes"
                            checked: root.s.watchChanges
                            onToggled: root.s.watchChanges = checked
                        }

                        Kirigami.Separator {
                            Kirigami.FormData.isSection: true
                            Kirigami.FormData.label: "Power"
                        }

                        QQC2.CheckBox {
                            text: "Pause while the screen is locked"
                            checked: root.s.pauseOnLock
                            onToggled: root.s.pauseOnLock = checked
                        }

                        QQC2.CheckBox {
                            text: "Pause while a fullscreen app is active"
                            checked: root.s.pauseOnFullscreen
                            onToggled: root.s.pauseOnFullscreen = checked
                        }

                        QQC2.CheckBox {
                            text: "Prevent sleep while playing"
                            checked: root.s.preventSleep
                            onToggled: root.s.preventSleep = checked
                        }

                        QQC2.CheckBox {
                            text: "Keep the screen on and unlocked while playing"
                            checked: root.s.preventScreenOff
                            onToggled: root.s.preventScreenOff = checked
                        }

                        SubCheckBox {
                            text: "Also while paused, if a video keeps playing"
                            enabled: root.s.preventSleep || root.s.preventScreenOff
                            checked: root.s.preventWhilePausedVideo
                            onToggled: root.s.preventWhilePausedVideo = checked
                        }

                        SubCheckBox {
                            text: "Also while paused on an image"
                            enabled: root.s.preventSleep || root.s.preventScreenOff
                            checked: root.s.preventWhilePausedImage
                            onToggled: root.s.preventWhilePausedImage = checked
                        }

                        QQC2.CheckBox {
                            text: "Pause after sleep or screen lock"
                            checked: root.s.pauseOnSleepOrLock
                            onToggled: root.s.pauseOnSleepOrLock = checked
                        }

                        QQC2.CheckBox {
                            text: "Hide after sleep or screen lock"
                            checked: root.s.hideOnSleepOrLock
                            onToggled: root.s.hideOnSleepOrLock = checked
                        }

                        Kirigami.Separator {
                            Kirigami.FormData.isSection: true
                            Kirigami.FormData.label: "Tray Icon"
                        }

                        TrayActionBox {
                            Kirigami.FormData.label: "Click:"
                            value: root.s.trayClick
                            onPicked: value => root.s.trayClick = value
                        }

                        TrayActionBox {
                            Kirigami.FormData.label: "Double-click:"
                            value: root.s.trayDoubleClick
                            onPicked: value => root.s.trayDoubleClick = value
                        }

                        Kirigami.Separator {
                            Kirigami.FormData.isSection: true
                            Kirigami.FormData.label: "Settings File"
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                            wrapMode: Text.WordWrap
                            text: "All settings, including every screen's tab. To apply settings to one screen, use Import on its tab."
                        }

                        SettingsFileButtons {
                            target: App
                            exportText: "Export All…"
                        }
                    }
                }
            }

            Repeater {
                model: App.profiles
                ProfilePage {
                    required property Profile modelData
                    profile: modelData
                }
            }
        }
    }
}
