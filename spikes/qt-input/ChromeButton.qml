import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design
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
    // A text button stands on a soft pill; quiet ones (the top bar, the
    // sidebar's footer) stay bare text until hovered.
    property bool quiet: false
    property string trailingText: ""
    property int textSize: Design.textControl
    // The gallery shows a state without a pointer: "hover", "down" or "focus".
    property string preview: ""
    readonly property bool showHover: hovered || preview === "hover"
    readonly property bool showDown: down || preview === "down"
    readonly property bool showFocus: visualFocus || preview === "focus"
    readonly property var colors: tones
    // Text on the accent: the theme's background or foreground, whichever contrasts more.
    readonly property color onAccent: {
        const light = c => { c = Qt.color(c); return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b }
        const accent = light(colors.accent)
        return Math.abs(accent - light(colors.background)) >= Math.abs(accent - light(colors.foreground)) ? colors.background : colors.foreground
    }
    readonly property bool iconOnly: iconName.length > 0 && text.length === 0
    readonly property bool filled: !quiet && !leftAligned && !iconOnly && text.length > 0
    // A wash of the foreground, so the pill works on any surface and theme.
    function wash(alpha) { const c = Qt.color(colors.foreground); return Qt.rgba(c.r, c.g, c.b, alpha) }
    implicitWidth: iconOnly ? 28 : Math.max(32, contentItem.implicitWidth + 20)
    implicitHeight: 32; padding: 0
    hoverEnabled: true; focusPolicy: Qt.StrongFocus
    Accessible.name: hint
    Accessible.description: keys
    contentItem: Item {
        implicitWidth: control.iconOnly ? 20 : label.implicitWidth + (trailing.implicitWidth > 0 ? trailing.implicitWidth + 16 : 0)
        opacity: control.enabled ? 1 : 0.38
        Text { id: label; x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 16 - (trailing.implicitWidth > 0 ? trailing.implicitWidth + 16 : 0); horizontalAlignment: control.leftAligned ? Text.AlignLeft : Text.AlignHCenter; elide: Text.ElideRight; visible: !control.iconOnly; text: control.text; textFormat: Text.PlainText; color: control.primary ? control.onAccent : control.colors.foreground; font.pixelSize: control.textSize; font.weight: control.primary ? Font.DemiBold : Font.Normal }
        Text { id: trailing; anchors.right: parent.right; anchors.rightMargin: Design.spaceL; anchors.verticalCenter: parent.verticalCenter; text: control.trailingText; color: control.colors.muted; font.pixelSize: Design.textSmall }
        ChromeIcon { anchors.centerIn: parent; width: 20; height: 20; visible: control.iconOnly; name: control.iconName; color: control.colors.foreground }
    }
    // Text buttons are pills: the main action filled with the accent, the
    // others a soft wash, a selected choice among them with a thin outline.
    // Rows and icon buttons are rounded rectangles like fields; the current
    // one is a wash without an outline. Hover deepens the wash; keyboard
    // focus is an accent ring.
    background: Rectangle {
        radius: control.filled || control.primary ? height / 2 : Design.radiusM
        color: control.primary ? (control.showDown ? Qt.darker(control.colors.accent, 1.15) : control.showHover ? Qt.lighter(control.colors.accent, 1.08) : control.colors.accent)
            : control.showDown ? control.wash(0.16) : control.showHover ? control.wash(control.selected ? 0.18 : control.filled ? 0.12 : 0.07)
            : control.selected ? control.wash(0.14) : control.filled ? control.wash(0.07) : control.showFocus ? control.wash(0.07) : "transparent"
        border.width: control.showFocus ? (control.primary ? Design.lineStrong : Design.line) : control.selected && control.filled ? Design.line : 0
        border.color: control.showFocus ? (control.primary ? control.colors.foreground : control.colors.accent) : control.wash(0.3)
        Behavior on color { ColorAnimation { duration: Design.fade } }
    }
    // A per-control tooltip stays themed and updates as its source text changes.
    ToolTip {
        parent: control; visible: control.hovered && !control.down && !control.selected && control.hint.length > 0; delay: 650
        text: control.hint; padding: Design.spaceM
        contentItem: Row {
            spacing: Design.spaceM
            Text { anchors.verticalCenter: parent.verticalCenter; text: control.hint; textFormat: Text.PlainText; color: control.colors.foreground; font.pixelSize: Design.textSmall }
            KeyChips { anchors.verticalCenter: parent.verticalCenter; keys: control.keys; tones: control.colors; plain: true }
        }
        background: Rectangle { color: control.colors.raised; radius: Design.radiusM; border.color: control.colors.border; SurfaceShadow { spread: 10; strength: 0.26 } }
    }
}
