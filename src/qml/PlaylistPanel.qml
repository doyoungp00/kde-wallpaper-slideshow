import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Slideshow

// Searchable "<number>. <filename>" list of the playback order. The search is the
// controller's filter, so playback stays within the results.
Rectangle {
    id: panel

    required property PlaybackController controller
    required property ProfileSettings settings
    readonly property Playlist playlist: controller.playlist
    property bool userScrolled: false
    property bool autoScrolling: false
    readonly property bool menuOpen: itemMenu.visible

    function showItemMenu(path) {
        itemMenu.path = path
        itemMenu.popup()
    }

    function scrollToCurrent() {
        if (userScrolled) {
            return
        }
        const row = playlist.filteredRow(controller.currentRow)
        if (row >= 0) {
            autoScrolling = true
            list.positionViewAtIndex(row, ListView.Center)
            autoScrolling = false
        }
    }

    // Breeze's view colors, for the Light and Dark choices of playlistTheme.
    readonly property var schemes: [null, {
            background: "#ffffff", text: "#232629", disabledText: "#707d8a", negativeText: "#da4453",
            highlight: "#3daee9", highlightedText: "#ffffff"
        }, {
            background: "#141618", text: "#fcfcfc", disabledText: "#a1a9b1", negativeText: "#da4453",
            highlight: "#3daee9", highlightedText: "#fcfcfc"
        }]
    readonly property var scheme: schemes[settings.playlistTheme] ?? null

    // The system color scheme, or a fixed Breeze one; the search field follows it too.
    // Undefined resets a color to the system one.
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    Kirigami.Theme.backgroundColor: scheme ? scheme.background : undefined
    Kirigami.Theme.alternateBackgroundColor: scheme ? scheme.background : undefined
    Kirigami.Theme.textColor: scheme ? scheme.text : undefined
    Kirigami.Theme.disabledTextColor: scheme ? scheme.disabledText : undefined
    Kirigami.Theme.negativeTextColor: scheme ? scheme.negativeText : undefined
    Kirigami.Theme.highlightColor: scheme ? scheme.highlight : undefined
    Kirigami.Theme.highlightedTextColor: scheme ? scheme.highlightedText : undefined
    Kirigami.Theme.hoverColor: scheme ? scheme.highlight : undefined
    Kirigami.Theme.focusColor: scheme ? scheme.highlight : undefined
    // Background of the panel and of the search field, at the playlist opacity.
    readonly property color translucentBackground: Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g,
                                                           Kirigami.Theme.backgroundColor.b, settings.playlistOpacity)
    color: translucentBackground
    border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)
    radius: 8

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            spacing: 4

            TextField {
                id: search
                Layout.fillWidth: true
                // The desktop style pins text fields to the system colors; take the panel's.
                Kirigami.Theme.inherit: true
                placeholderText: panel.controller.filterRegex ? "Search (regular expression)" : "Search"
                color: panel.controller.filterValid ? Kirigami.Theme.textColor : Kirigami.Theme.negativeTextColor
                text: panel.controller.filter
                onTextEdited: filterDelay.restart()
                Keys.onEscapePressed: panel.controller.filter = ""
                // Drawn by hand: Breeze paints fields opaque, and this one fades with the
                // panel while its text stays solid.
                background: Rectangle {
                    readonly property color ink: Kirigami.Theme.textColor
                    radius: 3
                    color: panel.translucentBackground
                    border.width: 1
                    border.color: search.activeFocus ? Kirigami.Theme.focusColor
                                : Qt.rgba(ink.r, ink.g, ink.b, search.hovered ? 0.5 : 0.3)
                }
            }

            ToolButton {
                id: regexButton
                Layout.preferredHeight: search.height
                Layout.preferredWidth: search.height
                text: ".*"
                font.bold: true
                // Not checkable, so the binding follows other screens' toggles too.
                checked: panel.controller.filterRegex
                onClicked: {
                    filterDelay.stop()
                    panel.autoScrolling = true
                    panel.controller.filter = search.text
                    panel.controller.filterRegex = !panel.controller.filterRegex
                    panel.autoScrolling = false
                }
                // Drawn by hand: the desktop style's flat button is hard to see as a toggle.
                contentItem: Text {
                    text: regexButton.text
                    font: regexButton.font
                    color: regexButton.checked ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    readonly property color ink: Kirigami.Theme.textColor
                    radius: 4
                    color: regexButton.checked ? Kirigami.Theme.highlightColor
                         : Qt.rgba(ink.r, ink.g, ink.b, regexButton.down ? 0.25 : regexButton.hovered ? 0.15 : 0.06)
                    opacity: regexButton.checked && regexButton.down ? 0.75 : 1
                    border.color: regexButton.checked ? Kirigami.Theme.highlightColor : Qt.rgba(ink.r, ink.g, ink.b, 0.45)
                    border.width: 1
                }
                ToolTip.text: "Use regular expression"
                ToolTip.visible: hovered
                ToolTip.delay: 600
            }
        }

        // Position in the queue playback follows: the search results, or the whole
        // playlist when the search matches nothing.
        Text {
            readonly property bool all: list.count === 0
            readonly property int total: all ? panel.playlist.count : list.count
            readonly property int index: {
                // filteredRow isn't notifiable; reading the search makes a new one re-run this.
                const search = [panel.controller.filter, panel.controller.filterRegex]
                const row = panel.controller.currentRow
                return (all ? row : panel.playlist.filteredRow(row)) + 1
            }

            text: "(" + (index > 0 ? index : "–") + "/" + total + ")"
            color: Kirigami.Theme.textColor
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: panel.playlist.filtered
            ScrollBar.vertical: ScrollBar {}

            onContentYChanged: {
                if (!panel.autoScrolling && (moving || dragging)) {
                    panel.userScrolled = true
                    resumeAutoScroll.restart()
                }
            }
            onCountChanged: Qt.callLater(panel.scrollToCurrent)

            delegate: Rectangle {
                id: row
                required property int number
                required property string name
                required property string relativePath
                required property string path
                required property bool current
                required property bool bad

                width: ListView.view.width - 10
                height: 24
                radius: 4
                color: current ? Kirigami.Theme.highlightColor
                     : mouse.containsMouse ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12) : "transparent"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    text: row.number + ". " + row.name
                    elide: Text.ElideMiddle
                    color: row.bad ? Kirigami.Theme.disabledTextColor : row.current ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    font.bold: row.current
                    font.strikeout: row.bad
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: event => {
                        if (event.button === Qt.RightButton) {
                            panel.showItemMenu(row.path)
                        } else {
                            panel.controller.jumpTo(row.number - 1)
                        }
                    }
                }

                ToolTip.visible: mouse.containsMouse && !panel.menuOpen
                ToolTip.delay: 600
                ToolTip.text: row.relativePath
            }
        }
    }

    Menu {
        id: itemMenu
        property string path

        MenuItem {
            text: "Show in File Manager"
            icon.name: "document-open-folder"
            onTriggered: App.revealFile(itemMenu.path)
        }
    }

    Timer {
        id: filterDelay
        interval: 150
        onTriggered: {
            panel.autoScrolling = true
            panel.controller.filter = search.text
            panel.autoScrolling = false
        }
    }

    // Auto-scrolling resumes a while after the user scrolls by hand.
    Timer {
        id: resumeAutoScroll
        interval: 5000
        onTriggered: {
            panel.userScrolled = false
            panel.scrollToCurrent()
        }
    }

    Connections {
        target: panel.controller
        function onCurrentChanged() { panel.scrollToCurrent() }
        // Another screen showing this profile changed the search.
        function onFilterChanged() {
            if (search.text !== panel.controller.filter) {
                search.text = panel.controller.filter
            }
            panel.scrollToCurrent()
        }
    }
}
