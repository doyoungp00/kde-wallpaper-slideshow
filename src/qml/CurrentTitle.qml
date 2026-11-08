import QtQuick
import QtQuick.Effects
import org.kde.kirigami as Kirigami
import Slideshow

// Filename of the item on screen, in the top right corner. In hover mode it stays
// hidden until the pointer comes near; the overlay then takes input over its area.
Item {
    id: title

    required property PlaybackController controller
    required property ProfileSettings settings
    readonly property bool onHover: settings.titleShowOnHover
    property bool hovered: false
    // Around the text, so the pointer reveals it a little before reaching it.
    readonly property int reach: 16

    width: label.width + 2 * reach
    height: label.height + 2 * reach
    x: parent.width - width - settings.titleMarginRight + reach
    y: settings.titleMarginTop - reach

    Text {
        id: label
        x: title.reach
        y: title.reach
        width: Math.min(implicitWidth, title.parent.width / 2)
        text: title.controller.currentPath.slice(title.controller.currentPath.lastIndexOf("/") + 1)
        elide: Text.ElideMiddle
        font.family: title.settings.titleFontFamily !== "" ? title.settings.titleFontFamily : Kirigami.Theme.defaultFont.family
        font.pixelSize: title.settings.titleFontSize
        color: title.settings.titleColor
        style: title.settings.titleOutline ? Text.Outline : Text.Normal
        styleColor: title.settings.titleOutlineColor
        opacity: !title.onHover || title.hovered ? 1 : 0
        layer.enabled: title.settings.titleShadow
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "black"
            shadowBlur: 0.6
            shadowHorizontalOffset: 2
            shadowVerticalOffset: 2
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    HoverHandler {
        enabled: title.onHover
        onHoveredChanged: title.hovered = hovered
    }
}
