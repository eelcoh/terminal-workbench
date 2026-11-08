import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design
MenuItem {
    id: control
    property var tones
    property string keyHint: ""
    // The gallery shows a state without a pointer: "hover" or "focus".
    property string preview: ""
    implicitHeight: 32; leftPadding: Design.spaceL; rightPadding: Design.spaceL
    Accessible.name: text
    Accessible.description: keyHint
    contentItem: Item {
        implicitWidth: label.implicitWidth + shortcut.implicitWidth + 36
        opacity: control.enabled ? 1 : 0.38
        Text { id: label; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; width: parent.width - shortcut.width - (shortcut.width > 0 ? 12 : 0); text: control.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: control.tones.foreground; font.pixelSize: Design.textControl }
        KeyChips { id: shortcut; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; keys: control.checkable ? "" : control.keyHint; tones: control.tones }
    }
    indicator: ChromeIcon { x: control.width - width - 12; y: (control.height-height)/2; width: 16; height: 16; name: "check"; color: control.tones.accent; visible: control.checkable && control.checked }
    background: Rectangle { radius: Design.radiusM; color: control.highlighted || control.preview === "hover" ? control.tones.hover : "transparent"; border.width: control.activeFocus || control.preview === "focus" ? Design.line : 0; border.color: control.tones.accent }
}
