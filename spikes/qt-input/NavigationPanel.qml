import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design

// One panel is reparented between the temporary drawer and the reserved column.
// Existing runtime observers supply facts; navigation never attaches or recovers.
// It always shows workspaces with their panes; the kind icons at its top filter
// the panes (shells, apps, hosts, agents), one kind at a time.
Item {
    id: navigation
    required property var host
    required property var session
    readonly property var kinds: host.navigationKinds
    function kindOn(kind) { return kinds.indexOf(kind) >= 0 }
    readonly property bool agents: kindOn("agents")
    readonly property bool hosts: kindOn("hosts")
    readonly property bool searchFocused: sidebarFilter.activeFocus
    // Workspace ID → opened or closed by hand; otherwise every workspace is open.
    property var expandedSpaces: ({})
    // The filter at the top narrows workspaces and panes by name, program, folder or agent.
    readonly property string filter: sidebarFilter.text.trim().toLocaleLowerCase()
    readonly property bool filtering: kinds.length > 0 || filter.length > 0
    function scrollTo(top, height) {
        if (top < list.contentY) list.contentY = top
        else if (top + height > list.contentY + list.height) list.contentY = Math.max(0, top + height - list.height)
    }
    // The kind filter's name for a pane's kind.
    function kindOf(i) {
        const kind = host.paneIdentity(host.panel(i)).kind
        return kind === "agent" ? "agents" : kind === "app" ? "apps" : kind === "host" ? "hosts" : "shells"
    }
    function kindShown(i) {
        if (kinds.length && kinds.indexOf(kindOf(i)) < 0) return false
        return true
    }
    function paneText(i) {
        const pane = host.panel(i), identity = host.paneIdentity(pane)
        return [identity.name, identity.place, pane ? host.spaces[pane.spaceIndex] : "", pane ? pane.facts.agent || "" : ""].join(" ").toLocaleLowerCase()
    }
    function textMatches(i) { return !filter || paneText(i).indexOf(filter) >= 0 }
    function paneMatches(i) { return kindShown(i) && textMatches(i) }
    // While filtering, the first matching pane; otherwise the current workspace.
    function focusFirstRow() {
        if (!filtering) { const current = workspaceRows.itemAt(host.workspace); if (current && current.visible) { focusWorkspace(host.workspace); return } }
        if (filtering) for (let i = 0; i < host.spaces.length; ++i) {
            const row = workspaceRows.itemAt(i)
            if (row && row.visible && row.expanded && row.panes.length) { row.focusPane(0); return }
        }
        for (let i = 0; i < host.spaces.length; ++i) { const row = workspaceRows.itemAt(i); if (row && row.visible) { focusWorkspace(i); return } }
    }
    function setExpanded(identity, open) { let values = Object.assign({}, expandedSpaces); values[identity] = open; expandedSpaces = values }
    // The agents the filters leave, for the agent commands and diagnostics.
    property var matches: []
    function focusSearch() { sidebarFilter.forceActiveFocus(Qt.TabFocusReason) }
    function rebuildMatches() {
        const selected = host.agentIndices.filter(i => { const pane = host.panel(i); return pane && textMatches(i) })
        if (selected.length === matches.length && selected.every((value, index) => value === matches[index])) return
        matches = selected
    }
    onFilterChanged: { rebuildMatches(); list.contentY = 0 }
    function clearFilters() { sidebarFilter.clear() }
    function escapeSearch() {
        if (host.drawerOpen) host.closeNavigation(); else { host.contentItem.forceActiveFocus(); host.restoreAfterPopup() }
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
        scrollTo(row.y, row.height)
        row.button.forceActiveFocus(Qt.TabFocusReason)
    }
    // A "new" action at the foot of the sidebar: a small icon (the kind filter's glyph
    // with a plus), its name and chord in the tooltip (owner, 2026-11-12).
    component FooterAction: ChromeButton {
        id: action
        property string label
        property string chord
        signal activated()
        tones: host.chrome; hint: label; keys: chord; height: 28
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { action.activated(); event.accepted = true }
            else event.accepted = false
        }
        onClicked: action.activated()
    }
    // No rail (owner, 2026-11-12): the kind filters head the panel, one kind at
    // a time; the shown kind again shows every pane.
    component KindButton: ChromeButton {
        required property string kind
        tones: host.chrome; height: 28; selected: navigation.kindOn(kind); onClicked: host.toggleKind(kind)
    }
    Row {
        id: kindRow; x: 18; y: 12; spacing: Design.spaceXs
        KindButton { objectName: "shellsButton"; kind: "shells"; iconName: "shell"; hint: "Shells" }
        KindButton { objectName: "appsButton"; kind: "apps"; iconName: "apps"; hint: "Apps" }
        KindButton { objectName: "hostsButton"; kind: "hosts"; iconName: "host"; hint: "Hosts" }
        KindButton {
            objectName: "agentsButton"; kind: "agents"; iconName: "agents"
            hint: host.waitingCount ? "Agents · " + host.waitingCount + " need input (last reports)" : "Agents"
            Rectangle {
                x: 19; y: -4; width: 16; height: 16; radius: 8; visible: host.waitingCount > 0; color: host.chrome.waiting
                Text { anchors.centerIn: parent; text: host.waitingCount; color: host.chrome.background; font.pixelSize: Design.textCaption }
            }
        }
    }
    Item { id: header; x: 14; y: kindRow.y + kindRow.height + Design.spaceM; width: 0; height: 0 }
    ContextSearch {
        id: sidebarFilter; objectName: "navigationFilter"; x: 14; y: header.y; width: parent.width - 28; tones: host.chrome
        placeholderText: "Filter workspaces and panes…"
        Accessible.name: "Filter workspaces and panes by name, program, folder or agent"
        onStepRequested: direction => navigation.focusFirstRow()
        onAccepted: if (!inputMethodComposing) navigation.focusFirstRow()
        onEscapePressed: { if (text.length) clear(); else navigation.escapeSearch() }
    }
    Flickable {
        id: list; x: 14; y: sidebarFilter.y + sidebarFilter.height + 12
        width: parent.width - 28; height: Math.max(0, createFooter.y - y - 12)
        clip: true; contentHeight: body.height; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Column {
            id: body; width: list.width; spacing: Design.spaceS
            Repeater {
                id: workspaceRows; model: host.spaces.length
                Item {
                    id: workspaceRow
                    required property int index
                    property alias button: workspaceEntry
                    readonly property string identity: host.spaceIds[index] || ""
                    // Panes grouped by workspace: the current one is open, the
                    // others are one row each until opened.
                    readonly property var allPanes: host.indices(index)
                    readonly property bool nameMatches: !navigation.filter || (host.spaces[index] || "").toLocaleLowerCase().indexOf(navigation.filter) >= 0
                    // The panes of the chosen kinds; with text, the matching ones, or all
                    // of a matching workspace's.
                    readonly property var kindPanes: allPanes.filter(i => navigation.kindShown(i))
                    readonly property var panes: nameMatches ? kindPanes : kindPanes.filter(i => navigation.textMatches(i))
                    // A kind filter keeps workspaces it empties, dimmed; text hides them.
                    visible: navigation.filter ? nameMatches || panes.length > 0 : true
                    readonly property bool emptied: navigation.filtering && panes.length === 0 && allPanes.length > 0
                    // Every workspace is open until closed; the choice stays while switching.
                    readonly property bool expanded: navigation.filtering ? panes.length > 0 && (!nameMatches || navigation.kinds.length > 0) || (navigation.expandedSpaces[identity] ?? true) : navigation.expandedSpaces[identity] ?? true
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
                        current: parent.index === host.workspace; expanded: workspaceRow.expanded && workspaceRow.panes.length > 0; text: host.spaces[parent.index]
                        count: navigation.filtering ? workspaceRow.panes.length : host.counts[parent.index] || 0
                        opacity: workspaceRow.emptied ? 0.5 : 1
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
                                // An agent's state follows its name, as the agent list showed it.
                                hint: (kind === "agent" ? [title + " · " + paneState, detail, host.spaces[workspaceRow.index] || ""] : [title, detail, paneState]).concat([identity.dirty ? "uncommitted changes" : ""]).filter(part => part).join(" · ")
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
                                // A working agent is the plain foreground: the accent marks only focus and selection.
                                iconTone: kind === "agent" ? (facts.needs_input ? "waiting" : facts.state === "Ready" ? "ready" : "foreground")
                                    : kind === "app" ? (pane && pane.appStopped ? "waiting" : "ready") : "muted"
                                onClicked: host.choosePane(modelData)
                                ChromeButton {
                                    objectName: "navigationAgentInfo" + paneRow.modelData; z: 2; visible: paneRow.kind === "agent"
                                    anchors.right: parent.right; anchors.rightMargin: Design.spaceXs; anchors.verticalCenter: parent.verticalCenter
                                    tones: host.chrome; iconName: "info"; hint: "Agent details for " + paneRow.facts.agent; focusPolicy: Qt.NoFocus
                                    opacity: paneRow.hovered || paneRow.activeFocus || hovered ? 1 : 0
                                    onClicked: if (paneRow.pane) host.openAgentInfo(paneRow.pane.terminalId)
                                }
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
            Label {
                objectName: "navigationNoMatches"; width: body.width; padding: Design.spaceM; color: host.chrome.muted; font.pixelSize: Design.textBody; wrapMode: Text.Wrap
                visible: navigation.filtering && !host.spaces.some((name, i) => host.indices(i).some(j => navigation.paneMatches(j)) || navigation.filter && name.toLocaleLowerCase().indexOf(navigation.filter) >= 0)
                text: navigation.kinds.length === 1 && navigation.agents && !host.agentIndices.length ? "No agents detected" : navigation.kinds.length === 1 && navigation.hosts && !navigation.filter ? "No host panes\nNew host pane… connects to a saved host or adds one." : "No matching panes"
            }
            Label {
                width: body.width; visible: !session.savedSession
                padding: Design.spaceM; topPadding: Design.space2xl; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: host.chrome.muted; font.pixelSize: Design.textSmall
            }
        }
    }
    // The pin at the foot's right: pinned, the sidebar keeps its column.
    ChromeButton {
        objectName: "navigationPin"; anchors.right: parent.right; anchors.rightMargin: 18; y: createFooter.y; height: 28
        tones: host.chrome; iconName: "pin"; selected: host.navigationPinned
        enabled: host.width >= 920 || host.navigationPinned
        hint: host.navigationPinned ? "Unpin navigation" : host.width < 920 ? "Widen the window to pin navigation" : "Pin navigation"
        onClicked: host.toggleNavigationPin()
    }
    // The "new" actions, each with its chord.
    Row {
        id: createFooter; x: 18; y: parent.height - height - 14; spacing: Design.spaceXs
        FooterAction {
            objectName: "newWorkspaceButton"; iconName: "workspaces-add"; label: "New workspace…"; chord: studyShortcuts.labels["new-workspace"] || ""; enabled: host.canCreateWorkspace
            onActivated: host.newWorkspace()
        }
        FooterAction {
            id: addApplications; objectName: "addApplicationsButton"; iconName: "apps-add"; label: "Add applications…"; chord: studyShortcuts.labels["apps"] || ""; enabled: host.spaces.length > 0
            onActivated: host.openCatalog()
        }
        FooterAction {
            objectName: "newHostButton"; iconName: "host-add"; label: "New host pane…"; chord: studyShortcuts.labels["new-host"] || ""
            onActivated: host.newHost()
        }
    }
}
