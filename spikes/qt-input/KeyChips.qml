import QtQuick
import Band.Design

// A shortcut from the live keymap as key chips: "Ctrl+Shift+P" or "⌘⇧P" shows
// one chip per key. Plain, it is muted text without chips: "⌘⇧P" on macOS,
// "Super+Alt+→" elsewhere. Its owner keeps the label text for accessibility.
Row {
    id: chips
    objectName: "keyChips"
    property string keys: ""
    property var tones
    property bool plain: false
    spacing: Design.spaceXs
    visible: keys.length > 0
    readonly property var names: ({Left: "←", Right: "→", Up: "↑", Down: "↓"})
    function parts(label) {
        let rest = label, result = []
        while (rest.length > 1 && "⌃⌘⌥⇧".indexOf(rest[0]) >= 0) { result.push(rest[0]); rest = rest.slice(1) }
        for (let match; (match = /^(Ctrl|Super|Meta|Alt|Shift)\+(?=.)/.exec(rest)); rest = rest.slice(match[0].length)) result.push(match[1])
        if (rest.length) result.push(names[rest] || rest)
        return result
    }
    Text {
        visible: chips.plain; text: chips.parts(chips.keys).join(chips.keys.indexOf("+") >= 0 ? "+" : "")
        textFormat: Text.PlainText; font.pixelSize: Design.textSmall; color: chips.tones.muted
    }
    Repeater {
        model: chips.plain ? [] : chips.parts(chips.keys)
        Rectangle {
            required property string modelData
            width: Math.max(height, key.implicitWidth + 10); height: 18; radius: Design.radiusS
            color: "transparent"; border.width: Design.line; border.color: chips.tones.border
            Text { id: key; anchors.centerIn: parent; text: parent.modelData; textFormat: Text.PlainText; font.pixelSize: Design.textCaption; color: chips.tones.muted }
        }
    }
}
