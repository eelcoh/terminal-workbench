import QtQuick
import QtQuick.Controls
import Band.Design

// One row of a SettingsCard: the name on the left, the value muted beside it,
// or the row's own content in place of the value. A hairline separates it
// from the row above.
Item {
    id: row
    required property var tones
    property string title: ""
    property string value: ""
    property bool selectable: false     // a path the reader may select and copy
    default property alias content: slot.data
    readonly property real valueX: Design.spaceL + (width - 2 * Design.spaceL) * 0.4
    width: parent ? parent.width : 0
    height: Math.max(name.implicitHeight, valueText.visible ? valueText.contentHeight : 0, slot.childrenRect.height) + 2 * Design.spaceL
    Rectangle {
        visible: !row.Positioner.isFirstItem
        x: Design.spaceL; width: row.width - 2 * Design.spaceL; height: Design.line; color: row.tones.border
    }
    Label {
        id: name; x: Design.spaceL; y: Math.max(Design.spaceL, (row.height - implicitHeight) / 2); width: row.valueX - x - Design.spaceM
        text: row.title; textFormat: Text.PlainText; wrapMode: Text.Wrap
        font.pixelSize: Design.textBody; color: row.tones.foreground
    }
    TextEdit {
        id: valueText; visible: slot.children.length === 0
        x: row.valueX; y: Math.max(Design.spaceL, (row.height - contentHeight) / 2); width: row.width - x - Design.spaceL
        text: row.value; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere
        readOnly: true; selectByMouse: row.selectable; activeFocusOnPress: row.selectable
        font.pixelSize: row.selectable ? Design.textSmall : Design.textBody; color: row.tones.muted
    }
    Item { id: slot; x: row.valueX; y: Design.spaceL; width: row.width - x - Design.spaceL; height: childrenRect.height }
}
