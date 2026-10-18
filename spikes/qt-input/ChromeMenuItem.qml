import QtQuick
import QtQuick.Controls
import Workbench.Probe
MenuItem {
    id: control
    property var tones
    property string keyHint: ""
    implicitHeight: 32; leftPadding: 12; rightPadding: 12
    Accessible.name: text
    contentItem: Item {
        implicitWidth: label.implicitWidth + shortcut.implicitWidth + 36
        opacity: control.enabled ? 1 : 0.38
        Text { id: label; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; width: parent.width - shortcut.width - (shortcut.width > 0 ? 12 : 0); text: control.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: control.tones.foreground; font.pixelSize: 14 }
        Text { id: shortcut; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: control.checkable ? "" : control.keyHint; color: control.tones.muted; font.pixelSize: 12 }
    }
    indicator: ChromeIcon { x: control.width - width - 12; y: (control.height-height)/2; width: 16; height: 16; name: "check"; color: control.tones.accent; visible: control.checkable && control.checked }
    background: Rectangle { radius: 6; color: control.highlighted ? control.tones.hover : "transparent"; border.width: control.activeFocus ? 1 : 0; border.color: control.tones.accent }
}
