import QtQuick
import Slideshow

// Bottom-layer surface for the playlist, edge controls and current filename. Its
// input region covers only those that take the pointer, so every other click reaches
// the desktop, except while the playlist's context menu is open.
Window {
    id: root

    required property Profile profile
    readonly property ProfileSettings settings: profile.settings
    readonly property var bars: [topBar, bottomBar, leftBar, rightBar]

    // Changes whenever an input rectangle moves, resizes or toggles.
    readonly property string maskKey: [playlist.visible, playlist.menuOpen, playlist.x, playlist.y, playlist.width, playlist.height,
                                       currentTitle.visible && currentTitle.onHover, currentTitle.x, currentTitle.y, currentTitle.width, currentTitle.height]
        .concat(bars.map(b => [b.visible, b.x, b.y, b.width, b.height].join())).join()

    function updateMask() {
        // An open menu takes every click, so one outside it closes it.
        if (playlist.menuOpen) {
            App.setInputRegion(root, [Qt.rect(0, 0, width, height)])
            return
        }
        const rects = []
        for (const item of [playlist].concat(bars)) {
            if (item.visible) {
                rects.push(Qt.rect(item.x, item.y, item.width, item.height))
            }
        }
        // Shown all the time, the filename lets clicks through; on hover it must see the pointer.
        if (currentTitle.visible && currentTitle.onHover) {
            rects.push(Qt.rect(currentTitle.x, currentTitle.y, currentTitle.width, currentTitle.height))
        }
        App.setInputRegion(root, rects)
    }

    color: "transparent"
    visible: false
    onMaskKeyChanged: updateMask()

    CurrentTitle {
        id: currentTitle
        controller: root.profile.controller
        settings: root.settings
        visible: root.settings.titleVisible
    }

    PlaylistPanel {
        id: playlist
        controller: root.profile.controller
        settings: root.settings
        visible: root.settings.playlistVisible
        width: Math.min(root.settings.playlistWidth, root.width - root.settings.playlistMarginX * 2)
        height: Math.min(root.settings.playlistHeight, root.height - root.settings.playlistMarginY * 2)
        // playlistCorner: 0 top left, 1 top right, 2 bottom left, 3 bottom right
        x: root.settings.playlistCorner % 2 ? root.width - width - root.settings.playlistMarginX : root.settings.playlistMarginX
        y: root.settings.playlistCorner >= 2 ? root.height - height - root.settings.playlistMarginY : root.settings.playlistMarginY
    }

    EdgeBar { id: topBar; controller: root.profile.controller; settings: root.settings; edge: Qt.TopEdge; visible: root.settings.edgeTop }
    EdgeBar { id: bottomBar; controller: root.profile.controller; settings: root.settings; edge: Qt.BottomEdge; visible: root.settings.edgeBottom }
    EdgeBar { id: leftBar; controller: root.profile.controller; settings: root.settings; edge: Qt.LeftEdge; visible: root.settings.edgeLeft }
    EdgeBar { id: rightBar; controller: root.profile.controller; settings: root.settings; edge: Qt.RightEdge; visible: root.settings.edgeRight }

    Component.onCompleted: updateMask()
}
