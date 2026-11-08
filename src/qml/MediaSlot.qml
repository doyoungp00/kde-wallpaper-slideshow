import QtQuick
import Slideshow

// One of the two buffers of a screen: shows an item when it is the front slot,
// and preloads the next item while it is the back slot.
Item {
    id: slot

    required property PlaybackController controller
    required property ProfileSettings settings
    property bool primary: false
    property bool front: false
    property string source
    property string kind
    property string path
    property bool hasVideo: false

    readonly property Item media: kind === "video" ? video.item : loader.item
    readonly property bool ready: media ? media.ready === true : false
    readonly property bool failed: media ? media.failed === true : false
    readonly property int fillMode: [Image.PreserveAspectCrop, Image.PreserveAspectFit, Image.Stretch][settings.scaleMode]

    function load(url, kind, path) {
        source = ""
        slot.kind = kind
        slot.path = path
        if (kind === "video") {
            hasVideo = true
        }
        source = url
    }

    visible: front

    Loader {
        id: loader
        anchors.fill: parent
        active: slot.source !== "" && slot.kind !== "video" && slot.width > 0
        sourceComponent: slot.kind === "animated" ? animatedComponent : imageComponent
    }

    // Kept once created: a player per file would be a new libmpv instance per file,
    // and their GPU memory isn't given back when they are destroyed.
    Loader {
        id: video
        anchors.fill: parent
        active: slot.hasVideo && slot.width > 0
        visible: slot.kind === "video"
        sourceComponent: videoComponent
    }

    Component {
        id: imageComponent
        Image {
            readonly property bool ready: status === Image.Ready
            readonly property bool failed: status === Image.Error
            // MediaImageProvider decodes at screen size for the scale mode, so big photos
            // stay small in memory, and loads in parallel with the other slots.
            source: "image://media/" + slot.settings.scaleMode + "/" + encodeURIComponent(slot.path)
            asynchronous: true
            cache: false
            fillMode: slot.fillMode
            sourceSize: Qt.size(slot.width, slot.height)
        }
    }

    Component {
        id: animatedComponent
        AnimatedImage {
            readonly property bool ready: status === Image.Ready
            readonly property bool failed: status === Image.Error
            source: slot.source
            cache: false
            autoTransform: true
            fillMode: slot.fillMode
            playing: slot.front
            paused: !slot.controller.animationsRunning
        }
    }

    Component {
        id: videoComponent
        MpvVideo {
            readonly property bool ready: loaded
            source: slot.kind === "video" ? slot.source : ""
            paused: !slot.front || !slot.controller.animationsRunning
            // Audio from the primary screen only, so mirrored screens don't echo.
            muted: !slot.front || !slot.primary || slot.settings.mute
            volume: slot.settings.volume
            scaleMode: slot.settings.scaleMode
            background: slot.settings.letterboxColor
            onDurationReported: ms => slot.controller.reportLoopLength(slot.path, ms)
        }
    }
}
