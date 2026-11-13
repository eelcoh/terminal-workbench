import QtQuick
import Band.Design

// A workspace in the sidebar, as a small bold section header over its
// terminals: its name (full color for the current workspace, muted for the
// others), the branch of a linked worktree, the agents needing input,
// uncommitted changes, and the terminal count while it is collapsed. The caret
// and the remove button sit beside it in NavigationPanel.
ChromeButton {
    id: entry
    property int row: 0                 // for test object names
    property string branch
    property int count: 0
    property int waiting: 0
    property bool dirty: false
    property bool current: false
    property bool expanded: true
    // Room kept free on the right for the caret and remove button laid over it.
    property int trailingRoom: 0
    height: 28; leftAligned: true
    trailingText: String(count)
    contentItem: Item {
        Row {
            anchors.left: parent.left; anchors.leftMargin: Design.spaceM; anchors.right: totals.left; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceS
            Text { id: workspaceName; width: Math.min(implicitWidth, parent.width - (workspaceBranch.visible ? Math.min(workspaceBranch.implicitWidth, parent.width * 0.45) + 6 : 0)); text: entry.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: entry.current ? entry.tones.foreground : entry.tones.muted; font.pixelSize: Design.textSmall; font.weight: Font.Bold }
            Text {
                id: workspaceBranch; objectName: "workspaceBranch" + entry.row; visible: entry.branch.length > 0; anchors.baseline: workspaceName.baseline
                width: Math.min(implicitWidth, parent.width - workspaceName.width - 6); text: entry.branch; textFormat: Text.PlainText; elide: Text.ElideRight; color: entry.tones.faint; font.pixelSize: Design.textCaption
            }
            // Activity right beside the name while collapsed; open, the terminal
            // rows carry it.
            Row {
                visible: !entry.expanded && entry.waiting > 0; spacing: Design.spaceXs; height: 18; anchors.verticalCenter: workspaceName.verticalCenter
                Rectangle { width: 5; height: 5; radius: 3; anchors.verticalCenter: parent.verticalCenter; color: entry.tones.waiting }
                Text { text: String(entry.waiting); color: entry.tones.waiting; font.pixelSize: Design.textCaption; anchors.verticalCenter: parent.verticalCenter }
            }
            Rectangle { objectName: "workspaceDirty" + entry.row; visible: !entry.expanded && entry.dirty; width: 6; height: 6; radius: 3; anchors.verticalCenter: workspaceName.verticalCenter; color: entry.tones.accent }
        }
        Row {
            id: totals; anchors.right: parent.right; anchors.rightMargin: Design.spaceL + entry.trailingRoom; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceM
            Text { visible: !entry.expanded; text: entry.trailingText; color: entry.tones.muted; font.pixelSize: Design.textCaption; height: 18; verticalAlignment: Text.AlignVCenter }
        }
    }
}
