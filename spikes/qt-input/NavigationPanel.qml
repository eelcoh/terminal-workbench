import QtQuick
import QtQuick.Controls

// One panel is reparented between the temporary drawer and the reserved column.
// Existing runtime observers supply facts; navigation never attaches or recovers.
Item {
    id: navigation
    required property var host
    required property var session
    readonly property bool agents: host.navigationSection === "agents"
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
        id: list; x: 14; y: header.y + header.height + 16; width: parent.width - 28; height: parent.height - y - 14
        clip: true; contentHeight: body.height; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Column {
            id: body; width: list.width; spacing: 6
            Repeater {
                model: navigation.agents ? 0 : host.spaces.length
                ChromeButton {
                    required property int index
                    objectName: "navigationWorkspace" + index
                    tones: host.chrome; width: body.width; height: 44; leftAligned: true
                    selected: index === host.workspace; trailingText: String(host.counts[index]); text: host.spaces[index]
                    onClicked: host.chooseWorkspace(index)
                }
            }
            ChromeButton {
                objectName: "newWorkspaceButton"; visible: !navigation.agents; width: body.width; height: visible ? 36 : 0
                tones: host.chrome; leftAligned: true; text: "New workspace…"; enabled: host.canCreateWorkspace
                onClicked: host.newWorkspace()
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
                    contentItem: Column {
                        spacing: 5
                        Text { width: parent.width; text: agentRow.facts.agent + " · " + agentRow.facts.state; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 13; color: agentRow.facts.needs_input ? host.chrome.waiting : agentRow.facts.state === "Ready" ? host.chrome.ready : host.chrome.foreground }
                        Text { width: parent.width; text: agentRow.space; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 12; color: host.chrome.muted }
                        Text { width: parent.width; text: agentRow.facts.title; textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: 12; color: host.chrome.muted }
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
}
