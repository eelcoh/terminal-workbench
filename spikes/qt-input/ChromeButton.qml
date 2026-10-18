import QtQuick
import QtQuick.Controls
import Workbench.Probe
Button {
    id: control
    property var tones
    property string iconName: ""
    property string hint: text
    property bool selected: false
    property bool leftAligned: false
    property string trailingText: ""
    property bool fixedPalette: false
    readonly property var colors: fixedPalette ? ({foreground: "#e0e6ef", muted: "#a5afbe", hover: "#323b4b", selected: "#2c3c51", accent: "#b4c9ec", raised: "#252d38", border: "#3a4657"}) : tones
    readonly property bool iconOnly: iconName.length > 0 && text.length === 0
    implicitWidth: iconOnly ? 28 : Math.max(32, contentItem.implicitWidth + 20)
    implicitHeight: 32; padding: 0
    hoverEnabled: true; focusPolicy: Qt.StrongFocus
    Accessible.name: hint
    contentItem: Item {
        implicitWidth: control.iconOnly ? 20 : label.implicitWidth + (trailing.implicitWidth > 0 ? trailing.implicitWidth + 16 : 0)
        opacity: control.enabled ? 1 : 0.38
        Text { id: label; x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 16 - (trailing.implicitWidth > 0 ? trailing.implicitWidth + 16 : 0); horizontalAlignment: control.leftAligned ? Text.AlignLeft : Text.AlignHCenter; elide: Text.ElideRight; visible: !control.iconOnly; text: control.text; textFormat: Text.PlainText; color: control.colors.foreground; font.pixelSize: 14 }
        Text { id: trailing; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: control.trailingText; color: control.colors.muted; font.pixelSize: 12 }
        ChromeIcon { anchors.centerIn: parent; width: 20; height: 20; visible: control.iconOnly; name: control.iconName; color: control.colors.foreground }
    }
    background: Rectangle { radius: 6; color: control.down || control.hovered || control.visualFocus ? control.colors.hover : control.selected ? control.colors.selected : "transparent"; border.width: control.visualFocus ? 1 : 0; border.color: control.colors.accent }
    // A per-control tooltip stays themed and updates as its source text changes.
    ToolTip {
        parent: control; visible: control.hovered && !control.down && !control.selected && control.hint.length > 0; delay: 650
        text: control.hint; padding: 8
        contentItem: Text { text: control.hint; textFormat: Text.PlainText; color: control.colors.foreground; font.pixelSize: 12 }
        background: Rectangle { color: control.colors.raised; radius: 6; border.color: control.colors.border }
    }
}
