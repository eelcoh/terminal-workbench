import QtQuick
import Workbench.Probe
import Band.Design

// A terminal under its workspace in the sidebar: its kind and the program (or
// the folder without one), uncommitted changes, and below in small text the
// folder and branch.
ChromeButton {
    id: entry
    property string kindIcon: "shell"
    property string iconTone: "muted"   // a tones key: muted, accent, ready, waiting
    property string title
    property string detail
    property bool dirty: false
    height: detail ? 40 : 30
    contentItem: Item {
        ChromeIcon {
            id: paneMark; x: 4; y: entry.detail ? 5 : (parent.height - height) / 2; width: 15; height: 15; name: entry.kindIcon
            color: entry.tones[entry.iconTone] || entry.tones.muted
        }
        Text {
            id: paneName; anchors.left: paneMark.right; anchors.leftMargin: Design.spaceM; anchors.verticalCenter: paneMark.verticalCenter
            width: Math.min(implicitWidth, parent.width - x - Design.spaceL)
            text: entry.title; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: entry.tones.foreground; font.pixelSize: Design.textBody
        }
        Rectangle {
            visible: entry.dirty; anchors.left: paneName.right; anchors.leftMargin: Design.spaceS; anchors.verticalCenter: paneName.verticalCenter
            width: 6; height: 6; radius: 3; color: entry.tones.accent
        }
        // The folder keeps its end when it has to give way.
        Text {
            visible: entry.detail.length > 0
            anchors.left: paneName.left; anchors.right: parent.right; anchors.rightMargin: Design.spaceM; y: paneMark.y + paneMark.height + 1
            text: entry.detail; textFormat: Text.PlainText; elide: Text.ElideLeft; font.pixelSize: Design.textCaption
            color: entry.tones.muted
        }
    }
}
