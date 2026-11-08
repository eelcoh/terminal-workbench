import QtQuick
import Band.Design

// A workspace in the sidebar: its name, the branch of a linked worktree, the
// agents needing input, uncommitted changes and its terminal count. The caret
// and the remove button sit beside it in NavigationPanel.
ChromeButton {
    id: entry
    property int row: 0                 // for test object names
    property string branch
    property int count: 0
    property int waiting: 0
    property bool dirty: false
    height: 44; leftAligned: true
    trailingText: String(count)
    contentItem: Item {
        Row {
            anchors.left: parent.left; anchors.leftMargin: Design.spaceM; anchors.right: totals.left; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceS
            Text { id: workspaceName; width: Math.min(implicitWidth, parent.width - (workspaceBranch.visible ? Math.min(workspaceBranch.implicitWidth, parent.width * 0.45) + 6 : 0)); text: entry.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: entry.tones.foreground; font.pixelSize: Design.textControl }
            Text {
                id: workspaceBranch; objectName: "workspaceBranch" + entry.row; visible: entry.branch.length > 0; anchors.baseline: workspaceName.baseline
                width: Math.min(implicitWidth, parent.width - workspaceName.width - 6); text: entry.branch; textFormat: Text.PlainText; elide: Text.ElideRight; color: entry.tones.muted; font.pixelSize: Design.textSmall
            }
        }
        Row {
            id: totals; anchors.right: parent.right; anchors.rightMargin: Design.spaceL; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceM
            Row {
                visible: entry.waiting > 0; spacing: Design.spaceXs; height: 18
                Rectangle { width: 5; height: 5; radius: 3; anchors.verticalCenter: parent.verticalCenter; color: entry.tones.waiting }
                Text { text: String(entry.waiting); color: entry.tones.waiting; font.pixelSize: Design.textSmall; anchors.verticalCenter: parent.verticalCenter }
            }
            Rectangle { objectName: "workspaceDirty" + entry.row; visible: entry.dirty; width: 6; height: 6; radius: 3; anchors.verticalCenter: parent.verticalCenter; color: entry.tones.accent }
            Text { text: entry.trailingText; color: entry.tones.muted; font.pixelSize: Design.textSmall; height: 18; verticalAlignment: Text.AlignVCenter }
        }
    }
}
