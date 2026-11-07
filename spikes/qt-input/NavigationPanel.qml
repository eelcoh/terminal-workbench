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
    readonly property bool apps: host.navigationSection === "apps"
    readonly property bool workspaces: !agents && !apps
    readonly property bool searchFocused: navigation.agents && agentSearch.activeFocus
    property bool needsOnly: false
    // Workspace ID → opened or closed by hand; otherwise only the current
    // workspace shows its panes.
    property var expandedSpaces: ({})
    Connections { target: host; function onWorkspaceChanged() { navigation.expandedSpaces = ({}) } }
    // The footer filter narrows Workspaces (names and panes) and Apps.
    readonly property string filter: sidebarFilter.text.trim().toLocaleLowerCase()
    onWorkspacesChanged: sidebarFilter.clear()
    onAppsChanged: sidebarFilter.clear()
    function paneText(i) {
        const pane = host.panel(i), identity = host.paneIdentity(pane)
        return [identity.name, identity.place, pane ? host.spaces[pane.spaceIndex] : ""].join(" ").toLocaleLowerCase()
    }
    function paneMatches(i) { return !filter || paneText(i).indexOf(filter) >= 0 }
    readonly property var shownApps: host.runningApps.filter(i => navigation.paneMatches(i))
    function focusFirstRow() {
        if (navigation.apps) { focusApp(0); return }
        for (let i = 0; i < host.spaces.length; ++i) { const row = workspaceRows.itemAt(i); if (row && row.visible) { focusWorkspace(i); return } }
    }
    function setExpanded(identity, open) { let values = Object.assign({}, expandedSpaces); values[identity] = open; expandedSpaces = values }
    property var matches: []
    function focusSearch() { agentSearch.forceActiveFocus(Qt.TabFocusReason) }
    function rebuildMatches() {
        const selected = host.agentIndices.filter(i => { const pane = host.panel(i); return pane && (!needsOnly || pane.facts.needs_input) && host.contextMatches(agentSearch.text, i) })
        if (selected.length === matches.length && selected.every((value, index) => value === matches[index])) return
        let focused = ""
        for (let i = 0; i < agentRows.count; ++i) { const row = agentRows.itemAt(i); if (row && row.activeFocus && row.pane) focused = row.pane.terminalId }
        matches = selected
        if (focused.length) Qt.callLater(function() {
            if (host.agentInfoOpen) return
            const index = matches.findIndex(i => { const pane = host.panel(i); return pane && pane.terminalId === focused })
            if (index >= 0) focusAgent(index); else focusSearch()
        })
    }
    function focusAgent(index) {
        if (!matches.length) return
        index = Math.max(0, Math.min(matches.length - 1, index))
        const row = agentRows.itemAt(index)
        if (!row) return
        if (row.y < list.contentY) list.contentY = row.y
        else if (row.y + row.height > list.contentY + list.height) list.contentY = Math.max(0, row.y + row.height - list.height)
        row.forceActiveFocus(Qt.TabFocusReason)
    }
    onAgentsChanged: if (!agents && agentSearch.activeFocus) host.contentItem.forceActiveFocus()
    onNeedsOnlyChanged: { rebuildMatches(); list.contentY = 0 }
    function showWaiting() { agentSearch.clear(); needsOnly = true; list.contentY = 0 }
    function clearFilters() { agentSearch.clear(); needsOnly = false }
    function escapeSearch() {
        if (agentSearch.text.length || needsOnly) clearFilters()
        else { if (host.drawerOpen) host.closeNavigation(); else { host.contentItem.forceActiveFocus(); host.restoreAfterPopup() } }
    }
    function focusApp(index) {
        const count = appRows.count
        if (!count) { addApplications.forceActiveFocus(Qt.TabFocusReason); return }
        const row = appRows.itemAt(Math.max(0, Math.min(count - 1, index)))
        if (row.y < list.contentY) list.contentY = row.y
        else if (row.y + row.height > list.contentY + list.height) list.contentY = Math.max(0, row.y + row.height - list.height)
        row.forceActiveFocus(Qt.TabFocusReason)
    }
    function focusWorkspace(index, direction) {
        const count = host.spaces.length
        if (!count) return
        let row = null
        for (let tries = 0; tries < count; ++tries, index += direction || 1) {
            row = workspaceRows.itemAt(((index % count) + count) % count)
            if (row && row.visible) break
        }
        if (!row || !row.visible) return
        if (row.y < list.contentY) list.contentY = row.y
        else if (row.y + row.height > list.contentY + list.height) list.contentY = Math.max(0, row.y + row.height - list.height)
        row.button.forceActiveFocus(Qt.TabFocusReason)
    }
    Row {
        id: header; x: 14; y: 14; spacing: 2
        // Each rail icon owns this panel's content; the title names it.
        Label {
            objectName: "navigationTitle"; width: 184; height: 28; leftPadding: 8; verticalAlignment: Text.AlignVCenter
            text: navigation.agents ? "Agents" : navigation.apps ? "Apps" : "Workspaces"; color: host.chrome.foreground; font.pixelSize: 14; font.weight: Font.DemiBold
        }
        ChromeButton {
            objectName: "navigationPin"; tones: host.chrome; iconName: "pin"; selected: host.navigationPinned
            enabled: host.width >= 920 || host.navigationPinned
            hint: host.navigationPinned ? "Unpin navigation" : host.width < 920 ? "Widen the window to pin navigation" : "Pin navigation"
            onClicked: host.toggleNavigationPin()
        }
    }
    Column {
        id: agentFilters; x: 14; y: header.y + header.height + 12; width: parent.width - 28; spacing: 6
        visible: navigation.agents
        ContextSearch {
            id: agentSearch; objectName: "navigationAgentSearch"; width: parent.width; tones: host.chrome
            placeholderText: "Find agents…"; Accessible.name: "Find agents by workspace, folder, Git or agent"
            onTextChanged: { navigation.rebuildMatches(); list.contentY = 0 }
            onStepRequested: direction => navigation.focusAgent(direction > 0 ? 0 : navigation.matches.length - 1)
            onAccepted: if (!inputMethodComposing && navigation.matches.length) host.chooseAgent(navigation.matches[0])
            onEscapePressed: navigation.escapeSearch()
        }
        Row {
            spacing: 4
            ChromeButton { objectName: "navigationNeedsFilter"; tones: host.chrome; text: navigation.needsOnly ? "Needs input" : "All states"; selected: navigation.needsOnly; hint: "Show agents that need input"; onClicked: navigation.needsOnly = !navigation.needsOnly }
            ChromeButton { objectName: "navigationClearFilters"; tones: host.chrome; text: "Clear filters"; visible: navigation.needsOnly || agentSearch.text.length > 0; onClicked: navigation.clearFilters() }
        }
        Label { width: parent.width; text: navigation.matches.length + " / " + host.agentIndices.length + " agents"; color: host.chrome.muted; font.pixelSize: 12 }
    }
    Flickable {
        id: list; x: 14; y: navigation.agents ? agentFilters.y + agentFilters.height + 12 : header.y + header.height + 16; width: parent.width - 28; height: Math.max(0, navigation.workspaces || navigation.apps ? createFooter.y - y - 12 : parent.height - y - 14)
        clip: true; contentHeight: body.height; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Column {
            id: body; width: list.width; spacing: 6
            Repeater {
                id: workspaceRows; model: navigation.workspaces ? host.spaces.length : 0
                Item {
                    id: workspaceRow
                    required property int index
                    property alias button: workspaceEntry
                    readonly property string identity: host.spaceIds[index] || ""
                    // Panes grouped by workspace: the current one is open, the
                    // others are one row each until opened.
                    readonly property var allPanes: host.indices(index)
                    readonly property bool nameMatches: !navigation.filter || (host.spaces[index] || "").toLocaleLowerCase().indexOf(navigation.filter) >= 0
                    // While filtering: the matching panes, or all of a matching workspace's.
                    readonly property var panes: nameMatches ? allPanes : allPanes.filter(i => navigation.paneMatches(i))
                    visible: nameMatches || panes.length > 0
                    readonly property bool expanded: navigation.filter ? panes.length > 0 && !nameMatches || (navigation.expandedSpaces[identity] ?? index === host.workspace) : navigation.expandedSpaces[identity] ?? index === host.workspace
                    width: body.width; height: 44 + (expanded && panes.length ? paneColumn.height + 4 : 0)
                    function focusPane(k) {
                        if (!panes.length) return
                        const row = paneRows.itemAt(Math.max(0, Math.min(panes.length - 1, k)))
                        if (!row) return
                        const top = workspaceRow.y + paneColumn.y + row.y
                        if (top < list.contentY) list.contentY = top
                        else if (top + row.height > list.contentY + list.height) list.contentY = Math.max(0, top + row.height - list.height)
                        row.forceActiveFocus(Qt.TabFocusReason)
                    }
                    ChromeButton {
                        objectName: "expandWorkspace" + parent.index
                        x: 0; anchors.verticalCenter: workspaceEntry.verticalCenter; width: 24; height: 28
                        visible: workspaceRow.panes.length > 0; tones: host.chrome; iconName: "chevron"; rotation: workspaceRow.expanded ? 0 : -90
                        focusPolicy: Qt.NoFocus
                        hint: (workspaceRow.expanded ? "Hide panes of " : "Show panes of ") + (host.spaces[parent.index] || "workspace")
                        onClicked: navigation.setExpanded(workspaceRow.identity, !workspaceRow.expanded)
                    }
                    ChromeButton {
                        id: workspaceEntry
                        Keys.onDownPressed: navigation.focusWorkspace(parent.index + 1, 1)
                        Keys.onUpPressed: navigation.focusWorkspace(parent.index - 1, -1)
                        // Right opens the workspace's panes and moves into them;
                        // Left closes them.
                        Keys.onRightPressed: { navigation.setExpanded(workspaceRow.identity, true); Qt.callLater(function() { workspaceRow.focusPane(0) }) }
                        Keys.onLeftPressed: navigation.setExpanded(workspaceRow.identity, false)
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Home) { navigation.focusWorkspace(0); event.accepted = true }
                            else if (event.key === Qt.Key_End) { navigation.focusWorkspace(host.spaces.length - 1, -1); event.accepted = true }
                            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.chooseWorkspace(parent.index); event.accepted = true }
                            else event.accepted = false
                        }
                        objectName: "navigationWorkspace" + parent.index
                        tones: host.chrome; x: 24; width: parent.width - 56; height: 44; leftAligned: true
                        selected: parent.index === host.workspace; trailingText: String(host.counts[parent.index]); text: host.spaces[parent.index]
                        readonly property int waiting: host.workspaceWaitingCounts[parent.index] || 0
                        // A closed workspace previews its panes in the tooltip.
                        hint: text + " · " + trailingText + " terminals" + (waiting ? " · " + waiting + " agents need input (last reports)" : "")
                            + (workspaceRow.expanded ? "" : workspaceRow.allPanes.slice(0, 8).map(i => { const identity = host.paneIdentity(host.panel(i)); return "\n" + [identity.name, identity.place].filter(part => part).join(" · ") }).join("")
                                + (workspaceRow.allPanes.length > 8 ? "\n…" : ""))
                        contentItem: Item {
                            Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.right: totals.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: workspaceEntry.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: host.chrome.foreground; font.pixelSize: 14 }
                            Row {
                                id: totals; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; spacing: 8
                                Row {
                                    visible: workspaceEntry.waiting > 0; spacing: 4; height: 18
                                    Rectangle { width: 5; height: 5; radius: 3; anchors.verticalCenter: parent.verticalCenter; color: host.chrome.waiting }
                                    Text { text: String(workspaceEntry.waiting); color: host.chrome.waiting; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                }
                                Text { text: workspaceEntry.trailingText; color: host.chrome.muted; font.pixelSize: 12; height: 18; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                        onClicked: host.chooseWorkspace(parent.index)
                    }
                    Column {
                        id: paneColumn; x: 24; y: 48; width: parent.width - 24; spacing: 2
                        visible: workspaceRow.expanded
                        Repeater {
                            id: paneRows; model: workspaceRow.expanded ? workspaceRow.panes : []
                            ChromeButton {
                                id: paneRow
                                required property int index
                                required property int modelData
                                readonly property var pane: host.panel(modelData)
                                readonly property var facts: pane ? pane.facts : ({title: "", agent: "", state: ""})
                                readonly property var identity: host.paneIdentity(pane)
                                readonly property string kind: identity.kind
                                // The program leads and the folder follows, as in pane headers;
                                // without a program name the folder leads.
                                readonly property string title: identity.name || identity.place
                                readonly property string detail: identity.name ? identity.place : ""
                                readonly property string paneState: kind === "agent" ? facts.state : kind === "app" && pane.appStopped ? "Stopped" : ""
                                objectName: "navigationPane" + workspaceRow.index + "_" + index
                                tones: host.chrome; width: paneColumn.width; height: 34
                                selected: modelData === host.selected && !host.overview
                                hint: [title, detail, paneState].filter(part => part).join(" · ")
                                Keys.onDownPressed: workspaceRow.focusPane(index + 1)
                                Keys.onUpPressed: { if (index === 0) workspaceEntry.forceActiveFocus(Qt.TabFocusReason); else workspaceRow.focusPane(index - 1) }
                                Keys.onLeftPressed: workspaceEntry.forceActiveFocus(Qt.TabFocusReason)
                                Keys.onPressed: event => {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.choosePane(modelData); event.accepted = true }
                                    else event.accepted = false
                                }
                                contentItem: Item {
                                    ChromeIcon {
                                        id: paneMark; x: 4; anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; name: paneRow.identity.icon
                                        color: paneRow.kind === "agent" ? (paneRow.facts.needs_input ? host.chrome.waiting : paneRow.facts.state === "Ready" ? host.chrome.ready : host.chrome.accent)
                                            : paneRow.kind === "app" ? (paneRow.pane && paneRow.pane.appStopped ? host.chrome.waiting : host.chrome.ready) : host.chrome.muted
                                    }
                                    // The program keeps its width; the folder gives way, keeping its end.
                                    Text {
                                        id: paneName; anchors.left: paneMark.right; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter
                                        width: Math.min(implicitWidth, parent.width - x - 8 - (paneRow.detail ? 48 : 0))
                                        text: paneRow.title; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: host.chrome.foreground; font.pixelSize: 13
                                    }
                                    Text {
                                        id: paneDetail; anchors.left: paneName.right; anchors.leftMargin: 8; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight; text: paneRow.detail; textFormat: Text.PlainText; elide: Text.ElideLeft; font.pixelSize: 11
                                        color: host.chrome.muted
                                    }
                                }
                                onClicked: host.choosePane(modelData)
                            }
                        }
                    }
                    ChromeButton {
                        objectName: "removeWorkspace" + parent.index
                        anchors.right: parent.right; anchors.verticalCenter: workspaceEntry.verticalCenter
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
                // Running apps in every workspace, like agents (the "list" trial).
                id: appRows; model: navigation.apps ? navigation.shownApps : []
                ChromeButton {
                    id: appRow
                    required property int index
                    required property int modelData
                    readonly property var pane: host.panel(modelData)
                    readonly property string space: pane ? host.spaces[pane.spaceIndex] || "" : ""
                    readonly property bool failed: pane ? pane.appStopped : false
                    objectName: "navigationApp" + index
                    tones: host.chrome; width: body.width; height: 52; padding: 10
                    selected: modelData === host.selected && !host.overview
                    hint: (pane ? pane.appName : "") + " · " + space + (failed ? " · Stopped" : "")
                    Keys.onDownPressed: navigation.focusApp(index + 1)
                    Keys.onUpPressed: navigation.focusApp(index - 1)
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Home) { navigation.focusApp(0); event.accepted = true }
                        else if (event.key === Qt.Key_End) { navigation.focusApp(appRows.count - 1); event.accepted = true }
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.choosePane(modelData); event.accepted = true }
                        else event.accepted = false
                    }
                    contentItem: Item {
                        Column {
                            anchors.left: parent.left; anchors.right: appState.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                            Text { width: parent.width; text: appRow.pane ? appRow.pane.appName : ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: host.chrome.foreground; font.pixelSize: 14 }
                            Text { width: parent.width; text: appRow.space; textFormat: Text.PlainText; elide: Text.ElideRight; color: host.chrome.muted; font.pixelSize: 12 }
                        }
                        Text { id: appState; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: appRow.failed ? "Stopped" : ""; color: host.chrome.waiting; font.pixelSize: 12 }
                    }
                    onClicked: host.choosePane(modelData)
                }
            }
            Label {
                width: body.width; visible: navigation.apps && appRows.count === 0
                padding: 8; text: host.runningApps.length && navigation.filter ? "No matching apps" : "No apps running\nAdd applications to open lazygit, btop and other terminal apps as panes."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: 13
            }
            Repeater {
                id: agentRows; model: navigation.agents ? navigation.matches : []
                ChromeButton {
                    id: agentRow
                    required property int index
                    required property int modelData
                    readonly property var pane: host.panel(modelData)
                    readonly property var facts: pane ? pane.facts : ({agent: "", state: "Unknown", title: ""})
                    readonly property string space: pane ? host.spaces[pane.spaceIndex] : ""
                    objectName: "navigationAgent" + index
                    tones: host.chrome; width: body.width; height: 86
                    Keys.onDownPressed: navigation.focusAgent(index + 1)
                    Keys.onUpPressed: { if (index === 0) navigation.focusSearch(); else navigation.focusAgent(index - 1) }
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Home) { navigation.focusAgent(0); event.accepted = true }
                        else if (event.key === Qt.Key_End) { navigation.focusAgent(navigation.matches.length - 1); event.accepted = true }
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.chooseAgent(modelData); event.accepted = true }
                        else event.accepted = false
                    }
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
                width: body.width; visible: navigation.agents && navigation.matches.length === 0
                padding: 8; text: host.agentIndices.length === 0 ? "No agents detected\nAgents in any workspace appear here." : navigation.needsOnly ? "No matching agents need input\nClear filters to see all agents." : "No matching agents\nTry another folder, workspace or agent."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: 13
            }
            Label {
                objectName: "navigationNoMatches"; width: body.width; padding: 8; color: host.chrome.muted; font.pixelSize: 13
                visible: navigation.workspaces && navigation.filter.length > 0 && !host.spaces.some((name, i) => name.toLocaleLowerCase().indexOf(navigation.filter) >= 0 || host.indices(i).some(j => navigation.paneMatches(j)))
                text: "No matching workspaces or panes"
            }
            Label {
                width: body.width; visible: !session.savedSession
                padding: 8; topPadding: 18; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: 12
            }
        }
    }
    Item {
        id: createFooter; x: 14; y: parent.height - height - 14; width: parent.width - 28; height: 94
        visible: navigation.workspaces || navigation.apps
        Rectangle { width: parent.width; height: 1; color: host.chrome.border }
        ContextSearch {
            id: sidebarFilter; objectName: "navigationFilter"; y: 12; width: parent.width; tones: host.chrome
            placeholderText: navigation.apps ? "Filter apps…" : "Filter workspaces and panes…"
            Accessible.name: navigation.apps ? "Filter running apps by name or workspace" : "Filter workspaces and panes by name, program or folder"
            onStepRequested: direction => navigation.focusFirstRow()
            onAccepted: if (!inputMethodComposing) navigation.focusFirstRow()
            onEscapePressed: { if (text.length) clear(); else navigation.escapeSearch() }
        }
        ChromeButton {
            id: addApplications; objectName: "addApplicationsButton"; y: 54; width: parent.width; height: 36; visible: navigation.apps
            tones: host.chrome; text: "Add applications…"; enabled: host.spaces.length > 0
            keys: studyShortcuts.labels["apps"] || ""
            contentItem: Row {
                spacing: 8; leftPadding: 8
                Item { width: 18; height: 36; ChromeIcon { anchors.centerIn: parent; width: 18; height: 18; name: "plus"; color: host.chrome.accent } }
                Text { height: 36; verticalAlignment: Text.AlignVCenter; text: "Add applications…"; color: host.chrome.accent; font.pixelSize: 13 }
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.openCatalog(); event.accepted = true }
                else event.accepted = false
            }
            onClicked: host.openCatalog()
        }
        ChromeButton {
            objectName: "newWorkspaceButton"; visible: navigation.workspaces; y: 54; width: parent.width; height: 36
            tones: host.chrome; text: "New workspace…"; enabled: host.canCreateWorkspace
            keys: studyShortcuts.labels["new-workspace"] || ""
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
