import QtQuick
import org.kde.kirigami as Kirigami
import Slideshow

// Player controls centred on one screen edge. Collapsed, only a thin strip as long
// as the controls takes input; hovering it fades in the buttons and progress bar.
Item {
    id: bar

    required property PlaybackController controller
    required property ProfileSettings settings
    required property int edge
    property bool revealed: false

    readonly property bool vertical: edge === Qt.LeftEdge || edge === Qt.RightEdge
    readonly property int buttonSize: 36
    readonly property int gap: 6
    readonly property int progressThickness: 6
    readonly property real buttonBorder: 1.5
    // The buttons and the progress bar touch, and neighbouring buttons share their
    // border; gap is only the margin around them.
    readonly property real length: 5 * buttonSize - 4 * buttonBorder + 2 * gap
    readonly property int fullThickness: buttonSize + progressThickness + 2 * gap
    // The size that takes input; the controls keep their full size so they can fade
    // out in place after the input area has shrunk back to the strip.
    readonly property int thickness: revealed ? fullThickness : settings.edgeStripWidth
    readonly property bool farEdge: edge === Qt.BottomEdge || edge === Qt.RightEdge
    // Buttons sit against the screen edge, the progress bar on the side facing inward.
    readonly property int buttonsOffset: farEdge ? fullThickness - gap - buttonSize : gap
    readonly property int progressOffset: farEdge ? buttonsOffset - progressThickness : buttonsOffset + buttonSize

    component BarButton: Rectangle {
        id: button
        property string iconName
        signal clicked

        width: bar.buttonSize
        height: bar.buttonSize
        color: tap.pressed ? "#d0d0d0" : tap.containsMouse ? "#ececec" : "white"
        border.color: "black"
        border.width: bar.buttonBorder

        Kirigami.Icon {
            anchors.centerIn: parent
            width: 20
            height: 20
            source: button.iconName
            isMask: true
            color: "black"
        }

        MouseArea {
            id: tap
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.clicked()
        }
    }

    x: vertical ? (edge === Qt.RightEdge ? parent.width - thickness : 0) : (parent.width - length) / 2
    y: vertical ? (parent.height - length) / 2 : (edge === Qt.BottomEdge ? parent.height - thickness : 0)
    width: vertical ? thickness : length
    height: vertical ? length : thickness

    HoverHandler {
        onHoveredChanged: {
            if (hovered) {
                hideDelay.stop()
                bar.revealed = true
            } else {
                hideDelay.restart()
            }
        }
    }

    Timer {
        id: hideDelay
        interval: bar.settings.edgeHideDelayMs
        onTriggered: bar.revealed = false
    }

    Item {
        x: bar.vertical && bar.farEdge ? bar.thickness - bar.fullThickness : 0
        y: !bar.vertical && bar.farEdge ? bar.thickness - bar.fullThickness : 0
        width: bar.vertical ? bar.fullThickness : bar.length
        height: bar.vertical ? bar.length : bar.fullThickness
        opacity: bar.revealed ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        Grid {
            x: bar.vertical ? bar.buttonsOffset : bar.gap
            y: bar.vertical ? bar.gap : bar.buttonsOffset
            rows: bar.vertical ? 5 : 1
            columns: bar.vertical ? 1 : 5
            spacing: -bar.buttonBorder

            BarButton { iconName: "media-skip-backward"; onClicked: bar.controller.first() }
            BarButton { iconName: "media-seek-backward"; onClicked: bar.controller.previous() }
            BarButton {
                iconName: bar.controller.paused ? "media-playback-start" : "media-playback-pause"
                onClicked: App.togglePause()
            }
            BarButton { iconName: "media-seek-forward"; onClicked: bar.controller.next() }
            BarButton { iconName: "media-skip-forward"; onClicked: bar.controller.last() }
        }

        Rectangle {
            id: track
            x: bar.vertical ? bar.progressOffset : bar.gap
            y: bar.vertical ? bar.gap : bar.progressOffset
            width: bar.vertical ? bar.progressThickness : bar.length - 2 * bar.gap
            height: bar.vertical ? bar.length - 2 * bar.gap : bar.progressThickness
            color: Qt.rgba(0, 0, 0, 0.35)
            border.color: "black"
            border.width: 1

            Rectangle {
                readonly property real fill: (bar.vertical ? track.height : track.width) * bar.controller.progress
                width: bar.vertical ? track.width : fill
                height: bar.vertical ? fill : track.height
                color: "white"
                border.color: "black"
                border.width: 1
            }
        }
    }
}
