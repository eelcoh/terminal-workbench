import QtQuick
import Workbench.Probe
import Band.Design

// A terminal under its workspace in the sidebar, on one line (after Rex): its
// kind, the program (or the folder without one), uncommitted changes, then the
// folder and branch, muted, giving way at the end.
ChromeButton {
    id: entry
    property string kindIcon: "shell"
    property string iconTone: "muted"   // a tones key: muted, foreground, ready, waiting
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
            width: 6; height: 6; radius: 3; color: entry.tones.changed
        }
        // The detail gives way a whole part at a time ("folder · branch" drops
        // the branch first); only its first part is cut short.
        FontMetrics { id: detailMetrics; font: paneDetail.font }
        Text {
            id: paneDetail; objectName: "paneDetail"
            visible: entry.detail.length > 0
            anchors.left: entry.dirty ? dirtyMark.right : paneName.right; anchors.leftMargin: Design.spaceS
            anchors.right: parent.right; anchors.rightMargin: Design.spaceL; anchors.baseline: paneName.baseline
            text: {
                const parts = entry.detail.split(" · ")
                for (let n = parts.length; n > 1; --n) {
                    const shown = parts.slice(0, n).join(" · ")
                    if (detailMetrics.advanceWidth(shown) <= width) return shown
                }
                return parts[0]
            }
            textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: Design.textBody
            color: entry.tones.faint
        }
    }
}
