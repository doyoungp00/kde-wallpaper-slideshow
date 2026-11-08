import QtQuick
import Slideshow

// Full-screen slideshow surface on the bottom layer. Three slots hold the current
// item and preload the ones either side of it, so a step back or forward is a hard
// cut to an already decoded item, and the old item stays up until the new one is
// ready: there is never a blank frame.
Window {
    id: root

    required property Profile profile
    readonly property PlaybackController controller: profile.controller
    property bool primary: false

    readonly property list<MediaSlot> slots: [slotA, slotB, slotC]
    property MediaSlot front: slotA

    // A slot whose load failed doesn't count, so the item is tried again later.
    function holding(url) {
        return slots.find(s => s.source === url && !s.failed)
    }

    // A slot that is neither showing nor holding one of the wanted items.
    function spareSlot(wanted) {
        return slots.find(s => s !== front && !wanted.includes(s.source))
    }

    function sync() {
        const c = controller
        if (c.currentPath === "") {
            for (const s of slots) {
                s.load("", "", "")
            }
            return
        }
        const current = c.currentUrl.toString()
        const neighbors = [[c.nextUrl.toString(), c.nextKind, c.nextPath], [c.previousUrl.toString(), c.previousKind, c.previousPath]]
            .filter(n => n[0] !== "" && n[0] !== current)
        const wanted = [current].concat(neighbors.map(n => n[0]))

        let slot = holding(current)
        if (!slot) {
            slot = spareSlot(wanted) ?? slots.find(s => s !== front)
            slot.load(current, c.currentKind, c.currentPath)
        }
        if (slot !== front) {
            if (!slot.ready) {
                return // its readyChanged calls sync again
            }
            front = slot
        }
        // Preload the neighbours only once the current item is up, so it loads first.
        for (const [url, kind, path] of neighbors) {
            if (!holding(url)) {
                spareSlot(wanted)?.load(url, kind, path)
            }
        }
    }

    function slotFailed(slot) {
        if (slot.failed) {
            controller.reportError(slot.path)
        }
    }

    // The real wallpaper shows through until the first item is ready.
    color: !front.ready ? "transparent" : profile.settings.scaleMode === 1 ? profile.settings.letterboxColor : "black"
    visible: false

    component Slot: MediaSlot {
        id: self
        anchors.fill: parent
        controller: root.controller
        settings: root.profile.settings
        primary: root.primary
        front: root.front === self
        onReadyChanged: Qt.callLater(root.sync)
        onFailedChanged: root.slotFailed(self)
    }

    Slot { id: slotA }
    Slot { id: slotB }
    Slot { id: slotC }

    Connections {
        target: root.controller
        function onCurrentMediaChanged() { Qt.callLater(root.sync) }
        function onCurrentChanged() { Qt.callLater(root.sync) }
        function onNeighborsChanged() { Qt.callLater(root.sync) }
    }

    Component.onCompleted: sync()
}
