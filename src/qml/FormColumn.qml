import QtQuick
import QtQuick.Layouts

// A ColumnLayout for a Kirigami.FormLayout row whose children come and go, such
// as a Repeater's delegates. FormLayout reads each row's Layout.minimumWidth and
// similar, and on a Qt layout that computes size hints from the layout's list of
// children without refreshing it first. Right after a delegate is deleted that
// list still holds it, and the read crashes. Wrapped in a plain Item, the row has
// no layout size hints to read, and the ColumnLayout only computes its own while
// laying itself out, after refreshing the list.
Item {
    default property alias content: column.data

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    ColumnLayout {
        id: column
        width: parent.width
    }
}
