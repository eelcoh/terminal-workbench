import QtQuick
import Workbench.Probe
import Band.Design

// A terminal under its workspace in the sidebar, on one line (after Rex): its
// kind, the program (or the folder without one), uncommitted changes, then the
// folder and branch, muted, giving way at the end.
ChromeButton {
    id: entry
    property string kindIcon: "shell"
    property string iconTone: "muted"   // a tones key: muted, accent, ready, waiting
    property string title
    property string detail
    property bool dirty: false
    height: 28
    contentItem: Item {
        ChromeIcon {
            id: paneMark; x: Design.spaceS; anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; name: entry.kindIcon
            color: entry.tones[entry.iconTone] || entry.tones.muted
        }
        Text {
            id: paneName; anchors.left: paneMark.right; anchors.leftMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, (parent.width - x - Design.spaceL) * (entry.detail ? 0.6 : 1))
            text: entry.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: entry.tones.foreground; font.pixelSize: Design.textBody
        }
        Rectangle {
            id: dirtyMark; visible: entry.dirty; anchors.left: paneName.right; anchors.leftMargin: Design.spaceS; anchors.verticalCenter: paneName.verticalCenter
            width: 6; height: 6; radius: 3; color: entry.tones.accent
        }
        Text {
            visible: entry.detail.length > 0
            anchors.left: entry.dirty ? dirtyMark.right : paneName.right; anchors.leftMargin: Design.spaceS
            anchors.right: parent.right; anchors.rightMargin: Design.spaceL; anchors.baseline: paneName.baseline
            text: entry.detail; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: Design.textBody
            color: entry.tones.muted
        }
    }
}
