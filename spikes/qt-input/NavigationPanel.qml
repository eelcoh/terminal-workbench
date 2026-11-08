import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design

// One panel is reparented between the temporary drawer and the reserved column.
// Existing runtime observers supply facts; navigation never attaches or recovers.
Item {
    id: navigation
    required property var host
    required property var session
    readonly property bool agents: host.navigationSection === "agents"
    readonly property bool apps: host.navigationSection === "apps"
    readonly property bool hosts: host.navigationSection === "hosts"
    readonly property bool workspaces: !agents && !apps && !hosts
    readonly property bool searchFocused: navigation.agents && agentSearch.activeFocus
    property bool needsOnly: false
    // Workspace ID → opened or closed by hand; otherwise only the current
    // workspace shows its panes.
    property var expandedSpaces: ({})
    // The filter at the top narrows Workspaces (names and panes) and Apps.
    readonly property string filter: sidebarFilter.text.trim().toLocaleLowerCase()
    onWorkspacesChanged: sidebarFilter.clear()
    onAppsChanged: sidebarFilter.clear()
    onHostsChanged: sidebarFilter.clear()
    // Reachability while the Hosts panel is open: on opening, then every minute.
    Timer {
        interval: 60000; repeat: true; triggeredOnStart: true; running: navigation.hosts && navigation.visible && host.hostDefinitions.length > 0
        onTriggered: studyHosts.check(host.hostDefinitions)
    }
    readonly property var shownHosts: host.hostDefinitions.filter(saved => !navigation.filter || (saved.name + " " + saved.destination).toLocaleLowerCase().indexOf(navigation.filter) >= 0)
    function focusHost(index) {
        const count = hostRows.count
        if (!count) { sidebarFilter.forceActiveFocus(Qt.TabFocusReason); return }
        const row = hostRows.itemAt(Math.max(0, Math.min(count - 1, index)))
        if (row.y < list.contentY) list.contentY = row.y
        else if (row.y + row.height > list.contentY + list.height) list.contentY = Math.max(0, row.y + row.height - list.height)
        row.button.forceActiveFocus(Qt.TabFocusReason)
    }
    function paneText(i) {
        const pane = host.panel(i), identity = host.paneIdentity(pane)
        return [identity.name, identity.place, pane ? host.spaces[pane.spaceIndex] : ""].join(" ").toLocaleLowerCase()
    }
    function paneMatches(i) { return !filter || paneText(i).indexOf(filter) >= 0 }
    readonly property var shownApps: host.runningApps.filter(i => navigation.paneMatches(i))
    function focusFirstRow() {
        if (navigation.apps) { focusApp(0); return }
        if (navigation.hosts) { focusHost(0); return }
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
    // The rail names the section and holds the pin, so the panel has no title
    // and the list starts at the top.
    Item { id: header; x: 14; y: 14; width: 0; height: 0 }
    // The filter heads the list in every section, as the agents search does.
    ContextSearch {
        id: sidebarFilter; objectName: "navigationFilter"; x: 14; y: header.y; width: parent.width - 28; tones: host.chrome
        visible: createFooter.visible
        placeholderText: navigation.apps ? "Filter apps…" : navigation.hosts ? "Filter hosts…" : "Filter workspaces and panes…"
        Accessible.name: navigation.apps ? "Filter running apps by name or workspace" : "Filter workspaces and panes by name, program or folder"
        onStepRequested: direction => navigation.focusFirstRow()
        onAccepted: if (!inputMethodComposing) navigation.focusFirstRow()
        onEscapePressed: { if (text.length) clear(); else navigation.escapeSearch() }
    }
    Column {
        id: agentFilters; x: 14; y: header.y; width: parent.width - 28; spacing: Design.spaceS
        visible: navigation.agents
        ContextSearch {
            id: agentSearch; objectName: "navigationAgentSearch"; width: parent.width; tones: host.chrome
            placeholderText: "Find agents…"; Accessible.name: "Find agents by workspace, folder, Git or agent"
            onTextChanged: { navigation.rebuildMatches(); list.contentY = 0 }
            onStepRequested: direction => navigation.focusAgent(direction > 0 ? 0 : navigation.matches.length - 1)
            onAccepted: if (!inputMethodComposing && navigation.matches.length) host.chooseAgent(navigation.matches[0])
            onEscapePressed: navigation.escapeSearch()
        }
        Item {
            width: parent.width; height: needsFilter.height
            Row {
                spacing: Design.spaceXs
                ChromeButton { id: needsFilter; objectName: "navigationNeedsFilter"; tones: host.chrome; text: navigation.needsOnly ? "Needs input" : "All states"; selected: navigation.needsOnly; hint: "Show agents that need input"; onClicked: navigation.needsOnly = !navigation.needsOnly }
                ChromeButton { objectName: "navigationClearFilters"; tones: host.chrome; text: "Clear filters"; visible: navigation.needsOnly || agentSearch.text.length > 0; onClicked: navigation.clearFilters() }
            }
            Label { anchors.right: parent.right; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; text: navigation.matches.length + " / " + host.agentIndices.length; color: host.chrome.muted; font.pixelSize: Design.textSmall }
        }
    }
    Flickable {
        id: list; x: 14; y: navigation.agents ? agentFilters.y + agentFilters.height + 12 : sidebarFilter.y + sidebarFilter.height + 12; width: parent.width - 28; height: Math.max(0, navigation.workspaces || navigation.apps || navigation.hosts ? createFooter.y - y - 12 : parent.height - y - 54)
        clip: true; contentHeight: body.height; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Column {
            id: body; width: list.width; spacing: Design.spaceS
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
                    // Every workspace is open until closed; the choice stays while switching.
                    readonly property bool expanded: navigation.filter ? panes.length > 0 && !nameMatches || (navigation.expandedSpaces[identity] ?? true) : navigation.expandedSpaces[identity] ?? true
                    width: body.width; height: 28 + (expanded && panes.length ? paneColumn.height + 2 : 0) + Design.spaceS
                    // The caret and the remove button show while the pointer is over the
                    // workspace or it has keyboard focus.
                    HoverHandler { id: rowHover }
                    readonly property bool actionsShown: rowHover.hovered || workspaceEntry.activeFocus || removeWorkspace.visualFocus
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
                        // The caret sits after the name, before the remove button.
                        x: parent.width - 56; z: 1; anchors.verticalCenter: workspaceEntry.verticalCenter; width: 24; height: 28
                        visible: workspaceRow.panes.length > 0; opacity: workspaceRow.actionsShown ? 1 : 0; tones: host.chrome; iconName: "chevron"; rotation: workspaceRow.expanded ? 0 : -90
                        focusPolicy: Qt.NoFocus
                        hint: (workspaceRow.expanded ? "Hide panes of " : "Show panes of ") + (host.spaces[parent.index] || "workspace")
                        onClicked: navigation.setExpanded(workspaceRow.identity, !workspaceRow.expanded)
                    }
                    SidebarWorkspace {
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
                        tones: host.chrome; x: 0; width: parent.width; trailingRoom: 56; row: parent.index
                        // The hover background spans the caret and remove button too.
                        preview: rowHover.hovered ? "hover" : ""
                        current: parent.index === host.workspace; expanded: workspaceRow.expanded && workspaceRow.panes.length > 0; count: host.counts[parent.index] || 0; text: host.spaces[parent.index]
                        waiting: host.workspaceWaitingCounts[parent.index] || 0
                        dirty: workspaceRow.allPanes.some(i => { const pane = host.panel(i); return pane && !!pane.facts.dirty })
                        // The branch of a linked worktree one of its panes is in.
                        branch: { for (const i of workspaceRow.allPanes) { const pane = host.panel(i); if (pane && pane.facts.worktree) return pane.facts.branch || pane.facts.worktree } return "" }
                        // A closed workspace previews its panes in the tooltip.
                        hint: text + (branch ? " · worktree " + branch : "") + " · " + trailingText + " terminals" + (waiting ? " · " + waiting + " agents need input (last reports)" : "") + (dirty ? " · uncommitted changes" : "")
                            + (workspaceRow.expanded ? "" : workspaceRow.allPanes.slice(0, 8).map(i => { const identity = host.paneIdentity(host.panel(i)); return "\n" + [identity.name, identity.place].filter(part => part).join(" · ") }).join("")
                                + (workspaceRow.allPanes.length > 8 ? "\n…" : ""))
                        onClicked: host.chooseWorkspace(parent.index)
                    }
                    Column {
                        id: paneColumn; x: 0; y: 30; width: parent.width; spacing: Design.space2xs
                        visible: workspaceRow.expanded
                        Repeater {
                            id: paneRows; model: workspaceRow.expanded ? workspaceRow.panes : []
                            SidebarPane {
                                id: paneRow
                                required property int index
                                required property int modelData
                                readonly property var pane: host.panel(modelData)
                                readonly property var facts: pane ? pane.facts : ({title: "", agent: "", state: ""})
                                readonly property var identity: host.paneIdentity(pane)
                                readonly property string kind: identity.kind
                                readonly property string paneState: kind === "agent" ? facts.state : kind === "app" && pane.appStopped ? "Stopped" : ""
                                objectName: "navigationPane" + workspaceRow.index + "_" + index
                                tones: host.chrome; width: paneColumn.width
                                selected: modelData === host.selected && !host.overview
                                hint: [title, detail, paneState, identity.dirty ? "uncommitted changes" : ""].filter(part => part).join(" · ")
                                Keys.onDownPressed: workspaceRow.focusPane(index + 1)
                                Keys.onUpPressed: { if (index === 0) workspaceEntry.forceActiveFocus(Qt.TabFocusReason); else workspaceRow.focusPane(index - 1) }
                                Keys.onLeftPressed: workspaceEntry.forceActiveFocus(Qt.TabFocusReason)
                                Keys.onPressed: event => {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.choosePane(modelData); event.accepted = true }
                                    else event.accepted = false
                                }
                                // The program leads and the folder follows, as in pane headers;
                                // without a program name the folder leads.
                                kindIcon: identity.icon; title: identity.name || identity.place; dirty: identity.dirty
                                detail: identity.name ? identity.place : ""      // the place already names repository and branch
                                iconTone: kind === "agent" ? (facts.needs_input ? "waiting" : facts.state === "Ready" ? "ready" : "accent")
                                    : kind === "app" ? (pane && pane.appStopped ? "waiting" : "ready") : "muted"
                                onClicked: host.choosePane(modelData)
                            }
                        }
                    }
                    ChromeButton {
                        id: removeWorkspace; objectName: "removeWorkspace" + parent.index; z: 1
                        anchors.right: parent.right; anchors.verticalCenter: workspaceEntry.verticalCenter
                        tones: host.chrome; iconName: "close"; opacity: !workspaceRow.actionsShown ? 0 : hovered || visualFocus ? 1 : 0.65
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
                    tones: host.chrome; width: body.width; height: 40; padding: Design.spaceM
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
                            anchors.left: parent.left; anchors.right: appState.left; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; spacing: 1
                            Text { width: parent.width; text: appRow.pane ? appRow.pane.appName : ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: host.chrome.foreground; font.pixelSize: Design.textBody }
                            Text { width: parent.width; text: appRow.space; textFormat: Text.PlainText; elide: Text.ElideRight; color: host.chrome.muted; font.pixelSize: Design.textCaption }
                        }
                        Text { id: appState; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: appRow.failed ? "Stopped" : ""; color: host.chrome.waiting; font.pixelSize: Design.textCaption }
                    }
                    onClicked: host.choosePane(modelData)
                }
            }
            Label {
                width: body.width; visible: navigation.apps && appRows.count === 0
                padding: Design.spaceM; text: host.runningApps.length && navigation.filter ? "No matching apps" : "No apps running\nAdd applications to open lazygit, btop and other terminal apps as panes."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: Design.textBody
            }
            Repeater {
                // Saved hosts, each with its open panes in any workspace.
                id: hostRows; model: navigation.hosts ? navigation.shownHosts : []
                Item {
                    id: hostRow
                    required property int index
                    required property var modelData
                    property alias button: hostEntry
                    readonly property var panes: host.hostPanes(modelData.id)
                    readonly property string reach: studyHosts.reachability[modelData.id] || ""
                    width: body.width; height: 40 + (panes.length ? hostPaneColumn.height + 4 : 0)
                    // Remove and connect show while the pointer is over the host or it has focus.
                    HoverHandler { id: hostHover }
                    readonly property bool actionsShown: hostHover.hovered || hostEntry.activeFocus
                    ChromeButton {
                        id: hostEntry; objectName: "navigationHost" + hostRow.index
                        tones: host.chrome; width: parent.width; height: 40; padding: Design.spaceM
                        hint: hostRow.modelData.name + " · " + hostRow.modelData.destination + (hostRow.panes.length ? " · " + hostRow.panes.length + " open" : "")
                            + (hostRow.reach === "reachable" ? " · reachable" : hostRow.reach === "unreachable" ? " · not reachable" : hostRow.reach === "unknown" ? " · reachability unknown" : "")
                        Keys.onDownPressed: navigation.focusHost(hostRow.index + 1)
                        Keys.onUpPressed: { if (hostRow.index === 0) sidebarFilter.forceActiveFocus(Qt.TabFocusReason); else navigation.focusHost(hostRow.index - 1) }
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.connectHost(hostRow.modelData.id); event.accepted = true }
                            else event.accepted = false
                        }
                        contentItem: Item {
                            ChromeIcon { id: hostIcon; x: 0; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; name: "host"; color: host.chrome.muted }
                            // Reachable: a ready dot; not reachable: a waiting dot; unknown: none.
                            Rectangle {
                                objectName: "hostReach" + hostRow.index; x: hostIcon.x + 12; y: hostIcon.y + 11; width: 8; height: 8; radius: 4
                                visible: hostRow.reach === "reachable" || hostRow.reach === "unreachable"; border.width: 1.5; border.color: host.chrome.surface
                                color: hostRow.reach === "reachable" ? host.chrome.ready : host.chrome.waiting
                            }
                            Column {
                                anchors.left: hostIcon.right; anchors.leftMargin: Design.spaceL; anchors.right: parent.right; anchors.rightMargin: hostRow.modelData.saved ? 64 : 34; anchors.verticalCenter: parent.verticalCenter; spacing: 1
                                Text { width: parent.width; text: hostRow.modelData.name; textFormat: Text.PlainText; elide: Text.ElideRight; color: host.chrome.foreground; font.pixelSize: Design.textBody; font.weight: Font.DemiBold }
                                Text { width: parent.width; text: hostRow.modelData.destination + (hostRow.modelData.directory ? " · " + hostRow.modelData.directory : ""); textFormat: Text.PlainText; elide: Text.ElideMiddle; color: host.chrome.muted; font.pixelSize: Design.textCaption }
                            }
                        }
                        onClicked: host.connectHost(hostRow.modelData.id)
                    }
                    // Hosts Band saved can be removed here; config.conf hosts stay as written.
                    ChromeButton {
                        objectName: "removeHost" + hostRow.index; z: 2; visible: !!hostRow.modelData.saved; opacity: hostRow.actionsShown ? 1 : 0
                        anchors.right: connectButton.left; anchors.verticalCenter: hostEntry.verticalCenter
                        tones: host.chrome; iconName: "close"; focusPolicy: Qt.NoFocus
                        hint: "Remove " + hostRow.modelData.name + " from Band's hosts"
                        onClicked: studyHosts.remove(hostRow.modelData.id)
                    }
                    ChromeButton {
                        id: connectButton; objectName: "connectHost" + hostRow.index; z: 2; opacity: hostRow.actionsShown ? 1 : 0
                        anchors.right: hostEntry.right; anchors.rightMargin: Design.spaceXs; anchors.verticalCenter: hostEntry.verticalCenter
                        tones: host.chrome; iconName: "plus"; focusPolicy: Qt.NoFocus; enabled: host.canCreateTerminal
                        hint: "Connect to " + hostRow.modelData.name + " in this workspace"
                        onClicked: host.connectHost(hostRow.modelData.id)
                    }
                    Column {
                        id: hostPaneColumn; x: Design.spaceL; y: 42; width: parent.width - Design.spaceL; spacing: Design.space2xs
                        Repeater {
                            model: hostRow.panes
                            ChromeButton {
                                required property int index
                                required property int modelData
                                readonly property var pane: host.panel(modelData)
                                objectName: "navigationHostPane" + hostRow.index + "_" + index
                                tones: host.chrome; width: hostPaneColumn.width; height: 30; leftAligned: true; textSize: Design.textBody
                                selected: modelData === host.selected && !host.overview
                                // The host is the row above; a pane row names its workspace, numbered when there are several.
                                text: pane ? (host.spaces[pane.spaceIndex] || "") + (hostRow.panes.length > 1 ? " · " + (index + 1) : "") : ""
                                hint: (pane ? pane.appName + " in " : "") + text
                                onClicked: host.choosePane(modelData)
                            }
                        }
                    }
                }
            }
            Label {
                width: body.width; visible: navigation.hosts && hostRows.count === 0
                padding: Design.spaceM; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: Design.textBody
                text: host.hostDefinitions.length && navigation.filter ? "No matching hosts" : "No hosts yet\nNew host… saves an SSH destination; [host.ID] sections in config.conf work too."
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
                    tones: host.chrome; width: body.width; height: 44
                    Keys.onDownPressed: navigation.focusAgent(index + 1)
                    Keys.onUpPressed: { if (index === 0) navigation.focusSearch(); else navigation.focusAgent(index - 1) }
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Home) { navigation.focusAgent(0); event.accepted = true }
                        else if (event.key === Qt.Key_End) { navigation.focusAgent(navigation.matches.length - 1); event.accepted = true }
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.chooseAgent(modelData); event.accepted = true }
                        else event.accepted = false
                    }
                    padding: Design.spaceM; selected: host.selected === modelData && !host.overview
                    hint: facts.agent + " · " + facts.state + " · " + space + " · " + facts.title
                    ChromeButton {
                        objectName: "navigationAgentInfo" + agentRow.index; z: 2
                        anchors.right: parent.right; anchors.rightMargin: Design.spaceXs; anchors.verticalCenter: parent.verticalCenter
                        tones: host.chrome; iconName: "info"; hint: "Agent details for " + agentRow.facts.agent
                        opacity: agentRow.hovered || agentRow.activeFocus || hovered || visualFocus ? 1 : 0
                        onClicked: if (agentRow.pane) host.openAgentInfo(agentRow.pane.terminalId)
                    }
                    contentItem: Column {
                        rightPadding: 30; spacing: 1
                        Text { width: parent.width - 30; text: agentRow.facts.agent + " · " + agentRow.facts.state; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: Design.textBody; color: agentRow.facts.needs_input ? host.chrome.waiting : agentRow.facts.state === "Ready" ? host.chrome.ready : host.chrome.foreground }
                        Text { width: parent.width - 30; text: [agentRow.space, agentRow.facts.title].filter(part => part).join(" · "); textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: Design.textCaption; color: host.chrome.muted }
                    }
                    onClicked: host.chooseAgent(modelData)
                }
            }
            Label {
                width: body.width; visible: navigation.agents && navigation.matches.length === 0
                padding: Design.spaceM; text: host.agentIndices.length === 0 ? "No agents detected\nAgents in any workspace appear here." : navigation.needsOnly ? "No matching agents need input\nClear filters to see all agents." : "No matching agents\nTry another folder, workspace or agent."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: Design.textBody
            }
            Label {
                objectName: "navigationNoMatches"; width: body.width; padding: Design.spaceM; color: host.chrome.muted; font.pixelSize: Design.textBody
                visible: navigation.workspaces && navigation.filter.length > 0 && !host.spaces.some((name, i) => name.toLocaleLowerCase().indexOf(navigation.filter) >= 0 || host.indices(i).some(j => navigation.paneMatches(j)))
                text: "No matching workspaces or panes"
            }
            Label {
                width: body.width; visible: !session.savedSession
                padding: Design.spaceM; topPadding: Design.space2xl; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: Design.textSmall
            }
        }
    }
    Item {
        id: createFooter; x: 14; y: parent.height - height - 14; width: parent.width - 28; height: 36
        visible: navigation.workspaces || navigation.apps || navigation.hosts
        ChromeButton {
            objectName: "newHostButton"; visible: navigation.hosts; y: 0; width: parent.width; height: 36
            tones: host.chrome; text: "New host…"; hint: "Save an SSH destination; [host.ID] in config.conf works too"
            contentItem: Row {
                spacing: Design.spaceM; leftPadding: Design.spaceM
                Item { width: 18; height: 36; ChromeIcon { anchors.centerIn: parent; width: 18; height: 18; name: "plus"; color: host.chrome.accent } }
                Text { height: 36; verticalAlignment: Text.AlignVCenter; text: "New host…"; color: host.chrome.accent; font.pixelSize: Design.textBody }
            }
            Keys.onReturnPressed: host.newHost()
            onClicked: host.newHost()
        }
        ChromeButton {
            id: addApplications; objectName: "addApplicationsButton"; y: 0; width: parent.width; height: 36; visible: navigation.apps
            tones: host.chrome; text: "Add applications…"; enabled: host.spaces.length > 0
            keys: studyShortcuts.labels["apps"] || ""
            contentItem: Row {
                spacing: Design.spaceM; leftPadding: Design.spaceM
                Item { width: 18; height: 36; ChromeIcon { anchors.centerIn: parent; width: 18; height: 18; name: "plus"; color: host.chrome.accent } }
                Text { height: 36; verticalAlignment: Text.AlignVCenter; text: "Add applications…"; color: host.chrome.accent; font.pixelSize: Design.textBody }
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.openCatalog(); event.accepted = true }
                else event.accepted = false
            }
            onClicked: host.openCatalog()
        }
        ChromeButton {
            objectName: "newWorkspaceButton"; visible: navigation.workspaces; y: 0; width: parent.width; height: 36
            tones: host.chrome; text: "New workspace…"; enabled: host.canCreateWorkspace
            keys: studyShortcuts.labels["new-workspace"] || ""
            contentItem: Row {
                opacity: host.canCreateWorkspace ? 1 : 0.38
                spacing: Design.spaceM; leftPadding: Design.spaceM
                Item { width: 18; height: 36; ChromeIcon { anchors.centerIn: parent; width: 18; height: 18; name: "plus"; color: host.chrome.accent } }
                Text { height: 36; verticalAlignment: Text.AlignVCenter; text: "New workspace…"; color: host.chrome.accent; font.pixelSize: Design.textBody }
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { host.newWorkspace(); event.accepted = true }
                else event.accepted = false
            }
            onClicked: host.newWorkspace()
        }
    }

}
