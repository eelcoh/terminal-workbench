import QtQuick
import Workbench.Probe
import Band.Design

// A terminal under its workspace in the sidebar: its kind, the program (or the
// folder without one), uncommitted changes and the folder on the right.
ChromeButton {
    id: entry
    property string kindIcon: "shell"
    property string iconTone: "muted"   // a tones key: muted, accent, ready, waiting
    property string title
    property string detail
    property bool dirty: false
    height: 34
    contentItem: Item {
        ChromeIcon {
            id: paneMark; x: 4; anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; name: entry.kindIcon
            color: entry.tones[entry.iconTone] || entry.tones.muted
        }
        // The program keeps its width; the folder gives way, keeping its end.
        Text {
            id: paneName; anchors.left: paneMark.right; anchors.leftMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width - x - 8 - (entry.detail ? 48 : 0))
            text: entry.title; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: entry.tones.foreground; font.pixelSize: Design.textBody
        }
        Rectangle {
            id: paneDirty; visible: entry.dirty; anchors.left: paneName.right; anchors.leftMargin: Design.spaceS; anchors.verticalCenter: parent.verticalCenter
            width: 6; height: 6; radius: 3; color: entry.tones.accent
        }
        Text {
            anchors.left: paneDirty.visible ? paneDirty.right : paneName.right; anchors.leftMargin: Design.spaceM; anchors.right: parent.right; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight; text: entry.detail; textFormat: Text.PlainText; elide: Text.ElideLeft; font.pixelSize: Design.textCaption
            color: entry.tones.muted
        }
    }
}
