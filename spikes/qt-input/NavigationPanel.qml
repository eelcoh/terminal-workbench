import QtQuick
import QtQuick.Controls
import Workbench.Probe

// One panel is reparented between the temporary drawer and the reserved column.
// Existing runtime observers supply facts; navigation never attaches or recovers.
Item {
    id: navigation
    required property var host
    required property var session
    readonly property bool agents: host.navigationSection === "agents"
    function focusWorkspace(index) {
        const count = host.spaces.length
        if (!count) return
        const row = workspaceRows.itemAt((index + count) % count)
        if (!row) return
        if (row.y < list.contentY) list.contentY = row.y
        else if (row.y + row.height > list.contentY + list.height) list.contentY = Math.max(0, row.y + row.height - list.height)
        row.button.forceActiveFocus(Qt.TabFocusReason)
    }
    Row {
        id: header; x: 14; y: 14; spacing: 2
        ChromeButton { objectName: "navigationWorkspaces"; tones: host.chrome; width: 106; text: "Workspaces"; selected: !navigation.agents; onClicked: host.navigationSection = "workspaces" }
        ChromeButton { objectName: "navigationAgents"; tones: host.chrome; width: 76; text: "Agents"; selected: navigation.agents; onClicked: host.navigationSection = "agents" }
        ChromeButton {
            objectName: "navigationPin"; tones: host.chrome; iconName: "pin"; selected: host.navigationPinned
            enabled: host.width >= 920 || host.navigationPinned
            hint: host.navigationPinned ? "Unpin navigation" : host.width < 920 ? "Widen the window to pin navigation" : "Pin navigation"
            onClicked: host.toggleNavigationPin()
        }
    }
    Flickable {
        id: list; x: 14; y: header.y + header.height + 16; width: parent.width - 28; height: Math.max(0, navigation.agents ? parent.height - y - 14 : createFooter.y - y - 12)
        clip: true; contentHeight: body.height; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Column {
            id: body; width: list.width; spacing: 6
            Repeater {
                id: workspaceRows; model: navigation.agents ? 0 : host.spaces.length
                Item {
                    required property int index
                    property alias button: workspaceEntry
                    readonly property string identity: host.spaceIds[index] || ""
                    width: body.width; height: 44
                    ChromeButton {
                        id: workspaceEntry
                        Keys.onDownPressed: navigation.focusWorkspace(parent.index + 1)
                        Keys.onUpPressed: navigation.focusWorkspace(parent.index - 1)
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Home) { navigation.focusWorkspace(0); event.accepted = true }
                            else if (event.key === Qt.Key_End) { navigation.focusWorkspace(host.spaces.length - 1); event.accepted = true }
                            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.chooseWorkspace(parent.index); event.accepted = true }
                            else event.accepted = false
                        }
                        objectName: "navigationWorkspace" + parent.index
                        tones: host.chrome; width: parent.width - 32; height: parent.height; leftAligned: true
                        selected: parent.index === host.workspace; trailingText: String(host.counts[parent.index]); text: host.spaces[parent.index]
                        onClicked: host.chooseWorkspace(parent.index)
                    }
                    ChromeButton {
                        objectName: "removeWorkspace" + parent.index
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        tones: host.chrome; iconName: "close"; opacity: hovered || visualFocus ? 1 : 0.65
                        hint: "Remove " + (host.spaces[parent.index] || "workspace") + "…"
                        enabled: !session.busy && host.dragSlot < 0
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.requestWorkspaceClose(parent.identity); event.accepted = true }
                            else event.accepted = false
                        }
                        onClicked: host.requestWorkspaceClose(parent.identity)
                    }
                }
            }
            Repeater {
                model: navigation.agents ? host.agentIndices : []
                ChromeButton {
                    id: agentRow
                    required property int index
                    required property int modelData
                    readonly property var pane: host.panel(modelData)
                    readonly property var facts: pane ? pane.facts : ({agent: "", state: "Unknown", title: ""})
                    readonly property string space: pane ? host.spaces[pane.spaceIndex] : ""
                    objectName: "navigationAgent" + index
                    tones: host.chrome; width: body.width; height: 86
                    padding: 10; selected: host.selected === modelData && !host.overview
                    hint: facts.agent + " · " + facts.state + " · " + space + " · " + facts.title
                    ChromeButton {
                        objectName: "navigationAgentInfo" + agentRow.index; z: 2
                        anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter
                        tones: host.chrome; iconName: "info"; hint: "Agent details for " + agentRow.facts.agent
                        onClicked: if (agentRow.pane) host.openAgentInfo(agentRow.pane.terminalId)
                    }
                    contentItem: Column {
                        rightPadding: 30; spacing: 5
                        Text { width: parent.width - 30; text: agentRow.facts.agent + " · " + agentRow.facts.state; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 13; color: agentRow.facts.needs_input ? host.chrome.waiting : agentRow.facts.state === "Ready" ? host.chrome.ready : host.chrome.foreground }
                        Text { width: parent.width - 30; text: agentRow.space; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 12; color: host.chrome.muted }
                        Text { width: parent.width - 30; text: agentRow.facts.title; textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: 12; color: host.chrome.muted }
                    }
                    onClicked: host.chooseAgent(modelData)
                }
            }
            Label {
                width: body.width; visible: navigation.agents && host.agentIndices.length === 0
                padding: 8; text: "No agents detected\nAgents in any workspace appear here."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: 13
            }
            Label {
                width: body.width; visible: !session.savedSession
                padding: 8; topPadding: 18; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: 12
            }
        }
    }
    Item {
        id: createFooter; x: 14; y: parent.height - height - 14; width: parent.width - 28; height: 50
        visible: !navigation.agents
        Rectangle { width: parent.width; height: 1; color: host.chrome.border }
        ChromeButton {
            objectName: "newWorkspaceButton"; y: 12; width: parent.width; height: 36
            tones: host.chrome; text: "New workspace…"; enabled: host.canCreateWorkspace
            hint: text + (studyShortcuts.labels["new-workspace"] ? " · " + studyShortcuts.labels["new-workspace"] : "")
            contentItem: Row {
                opacity: host.canCreateWorkspace ? 1 : 0.38
                spacing: 8; leftPadding: 8
                Item { width: 18; height: 36; ChromeIcon { anchors.centerIn: parent; width: 18; height: 18; name: "plus"; color: host.chrome.accent } }
                Text { height: 36; verticalAlignment: Text.AlignVCenter; text: "New workspace…"; color: host.chrome.accent; font.pixelSize: 13 }
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.newWorkspace(); event.accepted = true }
                else event.accepted = false
            }
            onClicked: host.newWorkspace()
        }
    }

}
