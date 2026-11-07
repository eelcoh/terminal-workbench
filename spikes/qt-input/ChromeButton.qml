import QtQuick
import QtQuick.Controls
import Workbench.Probe
Button {
    id: control
    property var tones
    property string iconName: ""
    property string hint: text
    // Its shortcut from the live keymap, shown as key chips in the tooltip.
    property string keys: ""
    property bool selected: false
    // The one main action of its view: filled with the theme's accent.
    property bool primary: false
    property bool leftAligned: false
    property string trailingText: ""
    property bool fixedPalette: false
    readonly property var colors: fixedPalette ? ({foreground: "#e0e6ef", muted: "#a5afbe", hover: "#323b4b", selected: "#2c3c51", accent: "#b4c9ec", raised: "#252d38", border: "#3a4657", background: "#101318"}) : tones
    // Text on the accent: the theme's background or foreground, whichever contrasts more.
    readonly property color onAccent: {
        const light = c => { c = Qt.color(c); return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b }
        const accent = light(colors.accent)
        return Math.abs(accent - light(colors.background)) >= Math.abs(accent - light(colors.foreground)) ? colors.background : colors.foreground
    }
    readonly property bool iconOnly: iconName.length > 0 && text.length === 0
    implicitWidth: iconOnly ? 28 : Math.max(32, contentItem.implicitWidth + 20)
    implicitHeight: 32; padding: 0
    hoverEnabled: true; focusPolicy: Qt.StrongFocus
    Accessible.name: hint
    Accessible.description: keys
    contentItem: Item {
        implicitWidth: control.iconOnly ? 20 : label.implicitWidth + (trailing.implicitWidth > 0 ? trailing.implicitWidth + 16 : 0)
        opacity: control.enabled ? 1 : 0.38
        Text { id: label; x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 16 - (trailing.implicitWidth > 0 ? trailing.implicitWidth + 16 : 0); horizontalAlignment: control.leftAligned ? Text.AlignLeft : Text.AlignHCenter; elide: Text.ElideRight; visible: !control.iconOnly; text: control.text; textFormat: Text.PlainText; color: control.primary ? control.onAccent : control.colors.foreground; font.pixelSize: 14; font.weight: control.primary ? Font.DemiBold : Font.Normal }
        Text { id: trailing; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: control.trailingText; color: control.colors.muted; font.pixelSize: 12 }
        ChromeIcon { anchors.centerIn: parent; width: 20; height: 20; visible: control.iconOnly; name: control.iconName; color: control.colors.foreground }
    }
    // Hover is a neutral tint, selection an accent tint, keyboard focus an accent ring.
    background: Rectangle {
        radius: 6
        color: control.primary ? (control.down ? Qt.darker(control.colors.accent, 1.15) : control.hovered ? Qt.lighter(control.colors.accent, 1.08) : control.colors.accent)
            : control.down || control.hovered ? control.colors.hover : control.selected ? control.colors.selected : control.visualFocus ? control.colors.hover : "transparent"
        border.width: control.visualFocus ? (control.primary ? 2 : 1) : 0; border.color: control.primary ? control.colors.foreground : control.colors.accent
    }
    // A per-control tooltip stays themed and updates as its source text changes.
    ToolTip {
        parent: control; visible: control.hovered && !control.down && !control.selected && control.hint.length > 0; delay: 650
        text: control.hint; padding: 8
        contentItem: Row {
            spacing: 8
            Text { anchors.verticalCenter: parent.verticalCenter; text: control.hint; textFormat: Text.PlainText; color: control.colors.foreground; font.pixelSize: 12 }
            KeyChips { anchors.verticalCenter: parent.verticalCenter; keys: control.keys; tones: control.colors }
        }
        background: Rectangle { color: control.colors.raised; radius: 6; border.color: control.colors.border }
    }
}
