import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Dialogs as NativeDialogs
import Workbench.Probe
import Band.Design
ApplicationWindow {
    id: window
    width: 1240; height: 760; minimumWidth: 680; minimumHeight: 440
    visible: true; title: studySession.savedSession ? "Band" : "Band · disposable sessions"; color: chrome.background
    // integrated-title-bar in config.conf folds the title bar into Band's top
    // bar: on Linux a frameless window with Band's own window buttons and
    // resize edges, on macOS the traffic lights inside the top bar (Qt 6.9+).
    readonly property bool appleTitle: studyConfiguration.integratedTitleBar && Design.apple && Qt.ExpandedClientAreaHint !== undefined
    readonly property bool ownTitle: studyConfiguration.integratedTitleBar && !Design.apple
    readonly property bool titleIntegrated: appleTitle || ownTitle
    // Read when About opens, so the content position is current.
    function titleBarReport() { return (appleTitle ? "folded in (macOS)" : ownTitle ? "folded in (frameless)"
        : !studyConfiguration.integratedTitleBar ? "system (integrated-title-bar is off)"
        : Design.apple ? "system (this Qt has no ExpandedClientAreaHint)" : "system")
        + " · " + Qt.platform.os + " · flags 0x" + (flags >>> 0).toString(16)
        + ("topPadding" in window ? " · content at " + contentItem.mapToItem(null, 0, 0).y.toFixed(0) + " px" : "") }
    // Room for the macOS traffic lights at the top left.
    readonly property int titleInset: appleTitle ? 72 : 0
    readonly property bool maximized: visibility === Window.Maximized || visibility === Window.FullScreen
    flags: appleTitle ? (Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint) : ownTitle ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window
    function toggleMaximized() { if (maximized) showNormal(); else showMaximized() }
    // macOS still draws the window title over a folded-in title bar; hide it
    // after the flags apply (a flag change can recreate the native window).
    // Qt 6.9+ pads an ApplicationWindow's content by the safe area, which with a
    // folded-in title bar is the title bar itself; Band's top bar fills it.
    function syncTitleText() {
        titleChrome.setHidden(window, appleTitle)
        if ("topPadding" in window) { window.topPadding = 0; window.leftPadding = 0; window.rightPadding = 0 }
    }
    onAppleTitleChanged: Qt.callLater(syncTitleText)
    // On macOS the application menu lives in the system menu bar (AppleMenus.qml)
    // instead of behind a button in the top bar. --menu-bar shows the same menus
    // inside the window elsewhere, to check them without a Mac.
    readonly property bool systemMenus: Design.apple || Qt.application.arguments.indexOf("--menu-bar") >= 0
    Component { id: appleMenus; AppleMenus {} }
    menuBar: systemMenus ? appleMenus.createObject(window, {host: window, registry: commandRegistry}) : null
    property int workspace: 0
    property int selected: 0
    property bool overview: false
    property string actionTarget: ""
    property string paneMenuTarget: ""
    property string actionKind: "close"
    property string actionContext: ""
    readonly property bool terminalActionOpen: terminalAction.visible
    function identityIndex(identity) { for (let i = 0; i < terminalModel.count; ++i) if (terminalModel.get(i).terminalId === identity) return i; return -1 }
    function syncItems() {
        let items = [], labels = []
        for (let i = 0; i < terminalModel.count; ++i) { const pane = panel(i); if (pane) items.push(pane.terminal); labels.push(captions.itemAt(i)) }
        terminalItems = items; overviewLabels = labels; rebuildOverview(); if (!overview) { if (selected >= 0) reveal(); else { deck.scrollTarget = 0; deck.contentX = 0 } }
    }
    property bool viewReady: false
    function scheduleViewSave() { if (viewReady && studyView.enabled) viewSaveTimer.restart() }
    function captureView() {
        let workspaces = {}, terminals = {}
        for (let w = 0; w < spaceIds.length; ++w) {
            const focus = remembered[w], validFocus = indices(w).indexOf(focus) >= 0
            const x = !overview && w === workspace ? deck.contentX : (positions[w] || 0)
            workspaces[spaceIds[w]] = {focus: validFocus ? terminalModel.get(focus).terminalId : "", x: Math.max(0,x), theme: studyThemes.snapshot(effectiveTheme(w)), theme_default: (workspaceThemes[w] ?? -1) < 0}
        }
        for (let i = 0; i < terminalModel.count; ++i) terminals[terminalModel.get(i).terminalId] = {width: widths[i] || 0.5, full: fullWidths[i] || false}
        return {version: 1, window: {width: Math.max(680,Math.min(16384,Math.round(width))), height: Math.max(440,Math.min(16384,Math.round(height)))},
            workspace: spaceIds[workspace] || "", terminal: selected >= 0 && selected < terminalModel.count ? terminalModel.get(selected).terminalId : "",
            navigation_default: navigationOverride === null, motion_default: motionOverride === null, navigation_pinned: navigationPinned, overview: overview, agents: agentsOnly, needs: needsOnly, motion: reducedMotion, overview_y: overview ? Math.max(0,deck.contentY) : 0,
            workspaces: workspaces, terminals: terminals}
    }
    function applySharedAppearance() {
        let themes = workspaceThemes.slice()
        for (let w = 0; w < spaceIds.length; ++w) {
            const choice = studyWorkspaceAppearance.choice(spaceIds[w])
            if (choice.version !== 1) continue
            const index = choice.default === true ? -1 : studyThemes.restoreSnapshot(choice.theme)
            if (index >= 0 || choice.default === true) themes[w] = index
        }
        workspaceThemes = themes
        let own = {}
        for (let i = 0; i < terminalModel.count; ++i) {
            const identity = terminalModel.get(i).terminalId, choice = studyTerminalAppearance.choice(identity)
            if (choice.version === 1 && choice.default !== true) { const index = studyThemes.restoreSnapshot(choice.theme); if (index >= 0) own[identity] = index }
        }
        terminalThemes = own
    }
    Connections { target: studyTerminalAppearance; function onChanged() { if (window.viewReady) Qt.callLater(window.applySharedAppearance) } function onReloadFailed() { window.fontWarningDismissed = false } }
    Connections { target: studyWorkspaceAppearance; function onChanged() { if (window.viewReady) Qt.callLater(window.applySharedAppearance) } function onReloadFailed() { window.fontWarningDismissed = false } }
    function persistView() { if (viewReady && studyView.enabled) studyView.save(captureView()) }
    function restoreView(saved) {
        if (!saved.version) return
        width = Math.max(minimumWidth, Math.min(saved.window.width, Screen.desktopAvailableWidth || saved.window.width))
        height = Math.max(minimumHeight, Math.min(saved.window.height, Screen.desktopAvailableHeight || saved.window.height))
        let sizes = widths.slice(), full = fullWidths.slice(), themes = [], focus = [], scroll = []
        for (let i = 0; i < terminalModel.count; ++i) {
            const value = saved.terminals[terminalModel.get(i).terminalId]
            if (value) { sizes[i] = value.width; full[i] = value.full }
        }
        widths = sizes; fullWidths = full
        for (let w = 0; w < spaceIds.length; ++w) {
            const value = saved.workspaces[spaceIds[w]], items = indices(w)
            const candidate = value ? identityIndex(value.focus) : -1
            focus.push(items.indexOf(candidate) >= 0 ? candidate : (items[0] ?? -1)); scroll.push(value ? value.x : 0)
            const theme = value ? (value.theme_default === true ? -1 : studyThemes.restoreSnapshot(value.theme)) : -1
            themes.push(theme)
            if (theme < 0 && value && value.theme_default !== true) studyView.invalidate("A saved workspace theme is invalid. Using default colors; view file left untouched.")
        }
        workspaceThemes = themes; remembered = focus; positions = scroll
        workspace = Math.max(0,spaceIds.indexOf(saved.workspace)); selected = identityIndex(saved.terminal)
        if (selected >= 0) workspace = terminalModel.get(selected).spaceIndex
        else selected = remembered[workspace] ?? -1
        navigationOverride = saved.navigation_default === true || saved.navigation_pinned === undefined ? null : saved.navigation_pinned
        motionOverride = saved.motion_default === true ? null : saved.motion; agentsOnly = saved.agents; needsOnly = saved.needs; overview = saved.overview
        rebuildOverview()
        Qt.callLater(function() {
            deck.scrollTarget = overview ? 0 : Math.max(0,Math.min(positions[workspace] || 0,stripWidth() - deck.width)); deck.contentX = deck.scrollTarget
            deck.contentY = overview ? Math.max(0,Math.min(saved.overview_y,deck.contentHeight-deck.height)) : 0
            if (!overview && selected >= 0) { reveal(); panel(selected).terminal.forceActiveFocus() }
            else window.contentItem.forceActiveFocus()
        })
    }
    Timer { id: viewSaveTimer; interval: 350; onTriggered: window.persistView() }
    onClosing: { viewSaveTimer.stop(); persistView() }
    onWidthChanged: scheduleViewSave()
    onHeightChanged: scheduleViewSave()
    onWorkspaceChanged: scheduleViewSave()
    onSelectedChanged: { scheduleViewSave(); if (terminalSearch.visible) terminalSearch.close() }
    onWidthsChanged: scheduleViewSave()
    onFullWidthsChanged: scheduleViewSave()
    onRememberedChanged: scheduleViewSave()
    onPositionsChanged: scheduleViewSave()
    onWorkspaceThemesChanged: scheduleViewSave()
    onSpaceIdsChanged: scheduleViewSave()
    onAgentsOnlyChanged: scheduleViewSave()
    onNeedsOnlyChanged: scheduleViewSave()
    onReducedMotionChanged: scheduleViewSave()
    property bool pendingTerminalAction: false
    property var stopTargets: []
    readonly property bool stopTargetsValid: stopTargets.length > 0 && stopTargets.every(identity => identityIndex(identity) >= 0)
    function actionExists() { return actionKind === "session-stop" ? stopTargets.length > 0 : actionKind === "workspace-close" ? spaceIds.indexOf(actionTarget) >= 0 : identityIndex(actionTarget) >= 0 }
    readonly property string workspaceRemovalContext: {
        const index = spaceIds.indexOf(actionTarget)
        return index < 0 ? "" : spaces[index] + " · " + counts[index] + (counts[index] === 1 ? " terminal" : " terminals")
    }
    function requestSessionStop() {
        if (studySession.busy || terminalAction.visible || terminalModel.count === 0 || createDialog.visible || appearance.visible || terminalSearch.visible || dragSlot >= 0) return
        let targets = []
        for (let i = 0; i < terminalModel.count; ++i) targets.push(terminalModel.get(i).terminalId)
        stopTargets = targets; actionTarget = ""; actionKind = "session-stop"; pendingTerminalAction = true
        if (drawer.visible) drawer.close()
        else if (appMenu.visible) appMenu.close()
        else { pendingTerminalAction = false; Qt.callLater(function() { if (actionExists()) terminalAction.open() }) }
    }
    function requestWorkspaceClose(identity) {
        if (studySession.busy || terminalAction.visible || spaceIds.indexOf(identity) < 0 || createDialog.visible || appearance.visible || dragSlot >= 0) return
        actionTarget = identity; actionKind = "workspace-close"; pendingTerminalAction = true
        if (drawer.visible) drawer.close()
        else if (appMenu.visible) appMenu.close()
        else { pendingTerminalAction = false; Qt.callLater(function() { if (actionExists()) terminalAction.open() }) }
    }
    function exitedTerminal(identity) {
        const i = identityIndex(identity), pane = i < 0 ? null : panel(i)
        return pane !== null && !pane.terminal.ready && pane.lifecycle.state === "stopped"
    }
    function closePane(identity) {
        if (studySession.busy || identityIndex(identity) < 0) return
        if (exitedTerminal(identity)) studySession.dismissTerminal(identity)
        else requestTerminalAction(identity,"close")
    }
    function requestTerminalAction(identity, kind) {
        if (studySession.busy || terminalAction.visible) return
        actionTarget = identity; actionKind = kind; const i = identityIndex(identity); actionContext = i < 0 ? "" : panel(i).facts.title + " · " + spaces[terminalModel.get(i).spaceIndex]; paneMenu.close()
        Qt.callLater(function() { if (identityIndex(actionTarget) >= 0) terminalAction.open() })
    }
    function performTerminalAction() {
        if (!actionExists() || studySession.busy) return
        if (actionKind === "session-stop") { if (stopTargetsValid) studySession.stopTerminals(stopTargets) }
        else if (actionKind === "workspace-close") studySession.closeWorkspace(actionTarget)
        else if (actionKind === "close") studySession.closeTerminal(actionTarget)
        else { const pane = panel(identityIndex(actionTarget)); studySession.reconnect(pane.terminal, actionTarget, true); terminalAction.close() }
    }
    property bool agentsOnly: false
    property bool needsOnly: false
    property string overviewQuery: ""
    readonly property bool overviewSearchFocused: overviewSearch.activeFocus
    readonly property bool navigationSearchFocused: navigation.searchFocused
    readonly property var navigationMatches: navigation.matches
    readonly property bool overviewFiltered: agentsOnly || needsOnly || overviewQuery.trim().length > 0
    onOverviewQueryChanged: { cancelDrag(); rebuildOverview(); deck.contentY = 0 }
    function contextMatches(query, i) {
        if (!query.trim().length) return true
        const pane = panel(i)
        if (!pane) return false
        const f = pane.facts, w = pane.spaceIndex
        const text = [spaces[w], directories[w], f.title, f.cwd, f.repository, f.branch, f.worktree, f.agent, f.state].join(" ").toLocaleLowerCase()
        return query.trim().toLocaleLowerCase().split(/\s+/).every(word => text.indexOf(word) >= 0)
    }
    function clearOverviewFilters() { overviewSearch.clear(); changeFilter(false, false) }
    property var overviewGroups: []
    property var overviewOrdinals: []
    readonly property int overviewColumns: deck.columns
    property int overviewMatches: 0
    property int waitingCount: 0
    readonly property int stoppedCount: {
        const facts = studySession.lifecycle; let count = 0
        for (let i = 0; i < terminalModel.count; ++i) if (terminalModel.get(i).appId === "" && (facts[terminalModel.get(i).terminalId] || {}).state === "stopped") ++count
        return count
    }
    property var waitingIdentities: []
    property var workspaceWaitingCounts: []
    property var agentIndices: []
    property var navigationOverride: null
    readonly property bool navigationPinned: navigationOverride === null ? studyConfiguration.navigationPinned : navigationOverride
    // The sidebar always shows workspaces; the rail filters their panes by kind.
    // Filters combine; none shows every pane.
    property var navigationKinds: []
    readonly property string navigationSection: navigationKinds.length ? navigationKinds.join("+") : "workspaces"
    // All panes, when already showing, hides a pinned column without unpinning it.
    property bool navigationHidden: false
    readonly property bool navigationDocked: navigationPinned && width >= 920 && !navigationHidden
    readonly property int navigationEdge: navigationDocked ? 288 : 44
    onNavigationPinnedChanged: scheduleViewSave()
    onNavigationDockedChanged: {
        if (navigationDocked) drawer.close()
        Qt.callLater(function() { if (!overview && selected >= 0) reveal() })
    }
    // The agents filter alone starts in the filter field, as the agent list did.
    function focusNavigation() { if (navigationKinds.length === 1 && navigationKinds[0] === "agents") navigation.focusSearch(); else navigation.focusFirstRow() }
    function openNavigation(kinds) {
        if (kinds !== undefined) navigationKinds = kinds
        navigationHidden = false
        // A rail click focuses its icon; switching sections returns focus to the panel.
        if (!navigationDocked && !drawer.visible) drawer.open()
        else Qt.callLater(focusNavigation)
    }
    function closeNavigation() { drawer.close() }
    readonly property bool navigationShown: drawer.visible || navigationDocked
    // One kind at a time (owner, 2026-11-09): an icon shows that kind alone;
    // clicking the shown kind again shows all panes.
    function toggleKind(kind) {
        if (!navigationShown) { openNavigation([kind]); return }
        navigationKinds = navigationKinds.length === 1 && navigationKinds[0] === kind ? [] : [kind]
    }
    // All panes clears the filters; when nothing is filtered it closes the sidebar.
    function showAllPanes() {
        if (navigationShown && !navigationKinds.length) {
            if (navigationDocked) { navigationHidden = true; Qt.callLater(restoreAfterPopup) }
            else drawer.close()
            return
        }
        navigation.clearFilters()
        openNavigation([])
    }
    function toggleNavigationPin() {
        navigationHidden = false
        navigationOverride = !navigationPinned
        if (navigationDocked) drawer.close()
        else drawer.open()
    }
    function jumpWaiting(direction) {
        rebuildOverview()
        let ordered = []
        for (let w = 0; w < spaces.length; ++w) for (const i of indices(w)) ordered.push(terminalModel.get(i).terminalId)
        const current = selected >= 0 && selected < terminalModel.count ? terminalModel.get(selected).terminalId : ""
        const identity = studyAgentInfo.nextWaiting(ordered, waitingIdentities, current, direction)
        const target = identityIndex(identity)
        if (target >= 0) chooseAgent(target)
    }
    function chooseAgent(i) {
        drawer.close(); enterPane(i)
        Qt.callLater(restoreAfterPopup)
    }
    readonly property var overviewGroupTops: { let tops = []; for (let w = 0; w < spaces.length; ++w) tops.push(deck.y + deck.groupTop(w) - deck.contentY); return tops }
    function rebuildOverview() {
        if (dragSlot >= 0) return
        let groups = [], positions = [], agents = [], waitingIds = [], waitingBySpace = [], total = 0, waiting = 0
        for (let i = 0; i < terminalModel.count; ++i) positions.push(-1)
        for (let w = 0; w < spaces.length; ++w) {
            let matches = [], spaceWaiting = 0
            for (let i of indices(w)) {
                const pane = panel(i), facts = pane ? pane.facts : ({agent: "", needs_input: false})
                if (facts.agent) agents.push(i)
                if (facts.agent && facts.needs_input) { ++waiting; ++spaceWaiting; waitingIds.push(terminalModel.get(i).terminalId) }
                if (overview && ((agentsOnly && !facts.agent) || (needsOnly && !facts.needs_input) || !contextMatches(overviewQuery, i))) continue
                positions[i] = matches.length; matches.push(i); ++total
            }
            groups.push(matches); waitingBySpace.push(spaceWaiting)
        }
        if (agents.length !== agentIndices.length || agents.some((value, index) => value !== agentIndices[index])) agentIndices = agents
        if (waitingIds.length !== waitingIdentities.length || waitingIds.some((value, index) => value !== waitingIdentities[index])) waitingIdentities = waitingIds
        if (waitingBySpace.length !== workspaceWaitingCounts.length || waitingBySpace.some((value, index) => value !== workspaceWaitingCounts[index])) workspaceWaitingCounts = waitingBySpace
        overviewGroups = groups; overviewOrdinals = positions; overviewMatches = total; waitingCount = waiting
        navigation.rebuildMatches()
        if (overview) deck.contentY = Math.max(0, Math.min(deck.contentY, deck.contentHeight - deck.height))
    }
    function overviewIndices(space) { return overviewGroups[space] || [] }
    function changeFilter(agents, needs) {
        cancelDrag(); agentsOnly = agents; needsOnly = needs; rebuildOverview(); deck.contentY = 0
    }
    onOverviewChanged: { Qt.callLater(rebuildOverview); scheduleViewSave() }
    function openAgents() { openNavigation(["agents"]) }
    WorkbenchActions { id: commandRegistry; host: window; session: studySession }
    readonly property bool shortcutSettingsOpen: shortcutSettings.visible
    readonly property bool shortcutRecording: shortcutSettings.recordingActive
    property string runtimeInfoTarget: ""
    property bool pendingRuntimeInfo: false
    readonly property bool runtimeInfoOpen: terminalInformation.visible
    readonly property var runtimeInfoPane: { const revision = catalogRevision; const i = identityIndex(runtimeInfoTarget); return i >= 0 ? panel(i) : null }
    function refreshRuntimeInfo() {
        if (identityIndex(runtimeInfoTarget) < 0) return
        studyRuntimeInfo.inspect(runtimeInfoTarget); studySession.inspect(runtimeInfoTarget)
    }
    function openRuntimeInfo(identity) {
        if (identityIndex(identity) < 0 || terminalInformation.visible || terminalAction.visible || createDialog.visible || appearance.visible || fontSettings.visible || shortcutSettings.visible || configurationDialog.visible || about.visible || help.visible) return
        runtimeInfoTarget = identity; pendingRuntimeInfo = true
        if (drawer.visible) drawer.close()
        else if (paneMenu.visible) paneMenu.close()
        else if (appMenu.visible) appMenu.close()
        else if (agentInformation.visible) agentInformation.close()
        else restoreAfterPopup()
    }
    property string agentInfoTarget: ""
    property bool pendingAgentInfo: false
    readonly property bool agentInfoOpen: agentInformation.visible
    readonly property var agentInfoPane: { const revision = catalogRevision; const i = identityIndex(agentInfoTarget); return i >= 0 ? panel(i) : null }
    function openAgentInfo(identity) {
        if (identityIndex(identity) < 0 || terminalInformation.visible || agentInformation.visible || terminalAction.visible || createDialog.visible || appearance.visible || fontSettings.visible || shortcutSettings.visible || configurationDialog.visible || about.visible || help.visible) return
        agentInfoTarget = identity
        if (drawer.visible || paneMenu.visible || appMenu.visible) {
            pendingAgentInfo = true
            if (drawer.visible) drawer.close()
            if (paneMenu.visible) paneMenu.close()
            if (appMenu.visible) appMenu.close()
        } else Qt.callLater(function() { if (identityIndex(agentInfoTarget) >= 0) agentInformation.open() })
    }
    property bool pendingShortcutSettings: false
    function openShortcutSettings() {
        if (shortcutSettings.visible || terminalAction.visible || createDialog.visible || appearance.visible || commands.visible || fontSettings.visible || drawer.visible || paneMenu.visible) return
        if (help.visible) help.close()
        if (appMenu.visible) { pendingShortcutSettings = true; appMenu.close() }
        else { studyShortcuts.begin(); shortcutSettings.open() }
    }
    property bool pendingFontSettings: false
    readonly property bool fontSettingsOpen: fontSettings.visible
    function openFontSettings() {
        if (shortcutSettings.visible || fontSettings.visible || terminalAction.visible || createDialog.visible || appearance.visible || commands.visible || configurationDialog.visible || help.visible) return
        if (appMenu.visible) { pendingFontSettings = true; appMenu.close() }
        else { studyFonts.begin(); fontSettings.open() }
    }
    property bool pendingCommands: false
    property string pendingCommand: ""
    property string commandTerminal: ""
    property string commandWorkspace: ""
    readonly property bool commandsOpen: commands.visible
    readonly property bool commandSearchFocus: commands.searchFocused
    readonly property int commandCurrent: commands.currentResult
    readonly property var commandMatches: commands.matches
    function openCommands() {
        if (terminalInformation.visible || agentInformation.visible || terminalAction.visible || createDialog.visible || appearance.visible || configurationDialog.visible || help.visible || drawer.visible || paneMenu.visible || shortcutSettings.visible || fontSettings.visible || commands.visible || dragSlot >= 0) return
        if (appMenu.visible) { pendingCommands = true; appMenu.close() }
        else showCommands()
    }
    function showCommands() {
        commandTerminal = selected >= 0 ? terminalModel.get(selected).terminalId : ""
        commandWorkspace = spaceIds[workspace] || ""
        commands.open()
    }
    function dispatchCommand(id) {
        const entry = commandRegistry.find(id)
        if (!entry || !entry.available) return
        switch (id) {
        case "terminal-info": openRuntimeInfo(terminalModel.get(selected).terminalId); break
        case "agent-info": openAgentInfo(terminalModel.get(selected).terminalId); break
        case "terminal-search": if (overview) overviewSearch.forceActiveFocus(); else openTerminalSearch(); break
        case "commands": openCommands(); break
        case "shortcut-settings": openShortcutSettings(); break
        case "font-settings": openFontSettings(); break
        case "font-increase": studyFonts.step(1); break
        case "font-decrease": studyFonts.step(-1); break
        case "font-reset": studyFonts.resetSize(); break
        case "new-terminal": newTerminal(); break
        case "new-workspace": newWorkspace(); break
        case "overview": toggleOverview(); break
        case "agents": openAgents(); break
        case "apps": openCatalog(); break
        case "next-waiting": jumpWaiting(1); break
        case "previous-waiting": jumpWaiting(-1); break
        case "previous-terminal": navigate(-1); break
        case "next-terminal": navigate(1); break
        case "previous-workspace": chooseWorkspace(workspace - 1); break
        case "next-workspace": chooseWorkspace(workspace + 1); break
        case "rename-workspace": editWorkspace("rename"); break
        case "workspace-folder": editWorkspace("folder"); break
        case "appearance": openAppearance(); break
        case "cycle-width": cycleWidth(); break
        case "full-width": toggleFull(); break
        case "move-left": reorderSelected(-1); break
        case "move-right": reorderSelected(1); break
        case "move-up": moveSelectedWorkspace(-1); break
        case "move-down": moveSelectedWorkspace(1); break
        case "session-stop": requestSessionStop(); break
        case "session-start": if (stoppedCount > 0) studySession.recoverAll(); break
        case "close-workspace": requestWorkspaceClose(spaceIds[workspace]); break
        case "close-terminal": closePane(terminalModel.get(selected).terminalId); break
        case "forward": panel(selected).terminal.forceActiveFocus(); panel(selected).terminal.forwardNextKey(); break;
        case "copy": panel(selected).terminal.copy(); break
        case "paste": panel(selected).terminal.paste(); break
        case "motion": motionOverride = !reducedMotion; break
        case "configuration": configurationDialog.open(); break
        case "configuration-reload": if (studyConfiguration.reload()) { moveNotice = "Preferences reloaded"; moveNoticeTimer.restart() }; break
        case "about": about.open(); break
        case "welcome": openWelcome(); break
        case "gallery": gallery.visible = true; gallery.forceActiveFocus(); break
        case "help": help.open(); break
        case "new-host": newHost(); break
        default: if (id.startsWith("host:")) connectHost(id.slice(5)); break
        }
    }
    property var motionOverride: null
    readonly property bool reducedMotion: motionOverride === null ? studyConfiguration.reducedMotion : motionOverride
    onNavigationOverrideChanged: scheduleViewSave()
    onMotionOverrideChanged: scheduleViewSave()
    property bool pendingWorkspaceDialog: false
    property string workspaceFormMode: "create"
    property string editingWorkspace: ""
    readonly property bool folderFocused: newFolder.activeFocus
    property var folderBrowser: null
    readonly property bool folderBrowserOpen: folderBrowser !== null && folderBrowser.visible
    readonly property url folderBrowserLocation: folderBrowser ? folderBrowser.currentFolder : ""
    readonly property url folderBrowserSelection: folderBrowser ? folderBrowser.selectedFolder : ""
    property bool pendingAppearance: false
    property var workspaceThemes: []
    property string appearanceWorkspace: ""
    // Non-empty while the picker edits one terminal's own colors.
    property string appearanceTerminal: ""
    property bool pendingTerminalColors: false
    property var terminalThemes: ({})
    property int previewTheme: 0
    property bool previewDefault: true
    readonly property bool appearanceOpen: appearance.visible
    readonly property bool themeHelpOpen: appearance.helpOpen
    readonly property bool applicationMenuOpen: appMenu.visible
    readonly property bool layoutMenuOpen: paneMenu.visible
    readonly property bool themeSearchFocus: appearance.searchFocused
    readonly property int themeMatchCount: appearance.matches.length
    readonly property var chrome: studyThemes.theme(themeIndexFor(workspace))
    function effectiveTheme(space) { const index = workspaceThemes[space] ?? -1; return index < 0 ? studyConfiguration.themeIndex : index }
    function themeIndexFor(space) { return appearance.visible && appearanceTerminal === "" && spaceIds[space] === appearanceWorkspace ? previewTheme : effectiveTheme(space) }
    function ownTheme(identity) { return terminalThemes[identity] ?? -1 }
    // A host pane's theme, by name from the theme list (case ignored); -1 when none.
    function hostTheme(identity) {
        const i = identityIndex(identity)
        const host = i < 0 ? null : hostOf(terminalModel.get(i).appId)
        if (!host || !host.theme) return -1
        const name = host.theme.toLocaleLowerCase(), entry = studyThemes.catalog.find(theme => theme.name.toLocaleLowerCase() === name)
        return entry ? entry.index : -1
    }
    // Terminal content colors: a terminal's own palette, else its host's theme,
    // else its workspace's. Pane frames and headers always follow the workspace.
    function terminalThemeIndex(identity, space) {
        const host = hostTheme(identity), fallback = host >= 0 ? host : themeIndexFor(space)
        if (appearance.visible && appearanceTerminal !== "" && identity === appearanceTerminal) return previewDefault ? fallback : previewTheme
        const own = ownTheme(identity)
        return own >= 0 ? own : fallback
    }
    readonly property string appearanceNote: {
        if (appearanceTerminal !== "") return "The pane frame keeps the workspace colors. Use workspace removes this terminal's own colors."
        const space = spaceIds.indexOf(appearanceWorkspace)
        let own = 0
        for (let i = 0; i < terminalModel.count; ++i) { const item = terminalModel.get(i); if (item.spaceIndex === space && ownTheme(item.terminalId) >= 0) ++own }
        return own === 0 ? "" : own === 1 ? "1 terminal here keeps its own colors." : own + " terminals here keep their own colors."
    }
    function openTerminalColors(identity) {
        if (identityIndex(identity) < 0 || terminalInformation.visible || terminalAction.visible || createDialog.visible || appearance.visible || fontSettings.visible || shortcutSettings.visible || configurationDialog.visible || about.visible || help.visible) return
        appearanceTerminal = identity; pendingTerminalColors = true
        if (paneMenu.visible) paneMenu.close()
        else if (appMenu.visible) appMenu.close()
        else restoreAfterPopup()
    }
    function showTerminalColors() {
        const index = identityIndex(appearanceTerminal)
        if (index < 0) { appearanceTerminal = ""; return }
        studyThemes.reloadDirectory()
        const space = terminalModel.get(index).spaceIndex
        appearanceWorkspace = spaceIds[space]
        if (!studyTerminalAppearance.begin(appearanceTerminal)) { appearanceTerminal = ""; return }
        applySharedAppearance()
        const own = ownTheme(appearanceTerminal)
        previewDefault = own < 0; previewTheme = own < 0 ? effectiveTheme(space) : own; appearance.open()
    }
    function openAppearance() {
        if (spaces.length === 0 || terminalAction.visible || overview || createDialog.visible || appearance.visible) return
        if (appMenu.visible) { pendingAppearance = true; appMenu.close() }
        else showAppearance()
    }
    function showAppearance() {
        if (spaces.length === 0) return
        studyThemes.reloadDirectory()
        appearanceTerminal = ""; appearanceWorkspace = spaceIds[workspace]
        if (!studyWorkspaceAppearance.begin(appearanceWorkspace)) return
        applySharedAppearance()
        previewTheme = effectiveTheme(workspace); previewDefault = (workspaceThemes[workspace] ?? -1) < 0; appearance.open()
    }
    property var widths: []
    property var fullWidths: []
    property var remembered: []
    property var positions: []
    property var spaces: []
    property var spaceIds: []
    property var directories: []
    property var counts: []
    property var terminalItems: []
    property var overviewLabels: []
    readonly property bool creationOpen: createDialog.visible
    readonly property bool drawerOpen: drawer.visible
    readonly property bool nameFocused: newName.activeFocus
    // The service bounds (catalog.rs); a workspace may hold any share of the terminals.
    readonly property int maxWorkspaces: 64
    readonly property int maxTerminals: 256
    readonly property int indicatorSlots: topbar.markerSlots
    readonly property int terminalTotal: { const revision = catalogRevision; return terminalModel.count }
    readonly property bool canCreateTerminal: spaces.length > 0 && terminalTotal < maxTerminals && !studySession.busy && dragSlot < 0
    readonly property bool canCreateWorkspace: spaces.length < maxWorkspaces && terminalTotal < maxTerminals && !studySession.busy && dragSlot < 0
    ListModel { id: terminalModel }
    property int catalogRevision: 0
    function indices(space) { const revision = catalogRevision; let result = []; for (let i = 0; i < terminalModel.count; ++i) if (terminalModel.get(i).spaceIndex === space && !endedApps[terminalModel.get(i).terminalId]) result.push(i); return result.sort((a,b) => terminalModel.get(a).ordinal - terminalModel.get(b).ordinal) }
    function ordinal(i) { const revision = catalogRevision; return i >= 0 && i < terminalModel.count ? terminalModel.get(i).ordinal : -1 }
    function applyCatalog(catalog, method) {
        if (method === "catalog.update") { cancelDrag(); paneMenu.close() }
        const selectedId = selected >= 0 && selected < terminalModel.count ? terminalModel.get(selected).terminalId : ""
        const oldWorkspace = workspace, oldWorkspaceId = spaceIds[workspace]
        let previous = {}
        for (let w = 0; w < spaceIds.length; ++w) {
            const i = remembered[w]
            previous[spaceIds[w]] = {focus: i >= 0 && i < terminalModel.count ? terminalModel.get(i).terminalId : "", x: positions[w] || 0, theme: workspaceThemes[w] ?? -1}
        }
        let incoming = []
        for (let space of catalog.workspaces) for (let terminal of space.terminals) incoming.push(terminal.id)
        let sizes = widths.slice(), full = fullWidths.slice()
        selected = -1
        for (let i = terminalModel.count - 1; i >= 0; --i) if (incoming.indexOf(terminalModel.get(i).terminalId) < 0) {
            terminalModel.remove(i); sizes.splice(i, 1); full.splice(i, 1)
        }
        widths = sizes; fullWidths = full
        let names = [], ids = [], folders = [], totals = []
        let focus = [], scroll = [], themes = []
        let added = -1
        for (let w = 0; w < catalog.workspaces.length; ++w) {
            const space = catalog.workspaces[w]
            names.push(space.name); ids.push(space.id); folders.push(space.directory || studySession.defaultDirectory); totals.push(space.terminals.filter(terminal => !endedApps[terminal.id]).length)
            const retained = previous[space.id]
            focus.push(retained ? identityIndex(retained.focus) : -1); scroll.push(retained ? retained.x : 0); themes.push(retained ? retained.theme : -1)
            for (let j = 0; j < space.terminals.length; ++j) {
                const terminal = space.terminals[j]
                let existing = -1
                for (let k = 0; k < terminalModel.count; ++k) if (terminalModel.get(k).terminalId === terminal.id) { existing = k; break }
                if (existing >= 0) {
                    terminalModel.setProperty(existing, "spaceIndex", w)
                    terminalModel.setProperty(existing, "ordinal", j)
                    continue
                }
                sizes.push(studyConfiguration.terminalWidth); full.push(false); widths = sizes; fullWidths = full
                terminalModel.append({terminalId: terminal.id, spaceIndex: w, ordinal: j, appId: terminal.app ? terminal.app.id : "", appName: terminal.app ? (terminal.name || terminal.app.id) : ""})
                added = terminalModel.count - 1
                if (focus[w] < 0) focus[w] = added
            }
        }
        spaces = names; spaceIds = ids; directories = folders; counts = totals; workspaceThemes = themes
        studyWorkspaceAppearance.setWorkspaces(ids)
        let terminalIds = []
        for (let i = 0; i < terminalModel.count; ++i) terminalIds.push(terminalModel.get(i).terminalId)
        studyTerminalAppearance.setWorkspaces(terminalIds)
        ++catalogRevision
        Qt.callLater(rebuildOverview)
        for (let w = 0; w < names.length; ++w) {
            const items = indices(w)
            if (items.indexOf(focus[w]) < 0) focus[w] = items.length ? items[0] : -1
        }
        remembered = focus; positions = scroll
        selected = identityIndex(selectedId)
        if (selected < 0) { workspace = ids.indexOf(oldWorkspaceId) >= 0 ? ids.indexOf(oldWorkspaceId) : Math.max(0, Math.min(oldWorkspace, names.length - 1)); selected = focus[workspace] ?? -1 }
        if (terminalInformation.visible && identityIndex(runtimeInfoTarget) < 0) terminalInformation.close()
        if (agentInformation.visible && identityIndex(agentInfoTarget) < 0) agentInformation.close()
        Qt.callLater(syncItems)
        if (selected >= 0) {
            workspace = terminalModel.get(selected).spaceIndex
            let values = remembered.slice(); values[workspace] = selected; remembered = values
            if (!overview) Qt.callLater(reveal)
        }
        let ended = {}
        for (const identity in endedApps) if (incoming.indexOf(identity) >= 0) ended[identity] = true
        endedApps = ended
        if (terminalAction.visible && !actionExists()) terminalAction.close()
        if (createDialog.visible && workspaceFormMode !== "create" && spaceIds.indexOf(editingWorkspace) < 0) createDialog.close()
        if (appearance.visible && (spaceIds.indexOf(appearanceWorkspace) < 0 || (appearanceTerminal !== "" && identityIndex(appearanceTerminal) < 0))) appearance.close()
        if (method === "workspace.create_at") { createDialog.close(); chooseWorkspace(spaces.length - 1); if (overview) toggleOverview() }
        if (method === "terminal.move" && followMove && selected >= 0) {
            followMove = false
            Qt.callLater(function() { if (overview) revealOverview(selected); else focusPane(selected) })
        }
        if (method === "session.stop" || method === "terminal.close" || method === "terminal.dismiss" || method === "workspace.close") {
            terminalAction.close(); Qt.callLater(restoreAfterPopup)
        }
        if (method === "workspace.rename" || method === "workspace.directory") createDialog.close()
        if (method === "terminal.create" && added >= 0) { if (overview) toggleOverview(); Qt.callLater(function() { focusPane(added) }) }
    }
    // Apps (docs/apps.md) are panes in the strip that run a command. When the
    // program exits normally its pane closes; after a failure it stays stopped,
    // with the exit code and last lines, until started again or closed.
    property var endedApps: ({})
    // The catalog merged with [app.ID] definitions, with installed state.
    readonly property var appDefinitions: studyConfiguration.apps
    onAppDefinitionsChanged: studyApps.setDefinitions(appDefinitions)
    readonly property var appEntries: studyApps.entries
    // Install panes that open their app when they finish: install ID → app ID.
    property var pendingInstalls: ({})
    property string openAfterCheck: ""
    // The Apps panel lists running apps in every workspace, like agents;
    // Add applications opens the catalog.
    property var pendingApp: null
    readonly property bool appCatalogOpen: appCatalog.visible
    readonly property string appCatalogCurrent: appCatalog.currentId
    readonly property int appCatalogCount: appCatalog.matchCount
    readonly property bool appSourcesOpen: appCatalog.visible && appCatalog.showSources
    readonly property var runningApps: {
        const revision = catalogRevision
        let result = []
        // Band's own panes (hosts, "_" IDs) are not apps.
        for (let i = 0; i < terminalModel.count; ++i) { const item = terminalModel.get(i); if (item.appId !== "" && !item.appId.startsWith("_") && !endedApps[item.terminalId]) result.push(i) }
        return result.sort((a, b) => (terminalModel.get(a).spaceIndex - terminalModel.get(b).spaceIndex) || (terminalModel.get(a).ordinal - terminalModel.get(b).ordinal))
    }
    function showApps() { openNavigation(["apps"]) }
    // SSH hosts from config.conf ([host.ID]); a host pane runs ssh as a command
    // terminal with the app ID "_host-ID" (then "_host-ID_2", …). See docs/hosts.md.
    // config.conf hosts with Band's saved hosts over them (marked "saved").
    readonly property var hostDefinitions: { const own = studyHosts.saved; return studyHosts.merge(studyConfiguration.hosts) }
    readonly property bool hostDialogOpen: hostDialog.visible
    readonly property bool hostChooserOpen: hostChooser.visible
    // Choosers that keep keyboard focus while panes open or close beneath them.
    readonly property bool choosing: appCatalog.visible || commands.visible || hostChooser.visible || hostDialog.visible
    // New host pane: choose a saved host, or add one when there are none.
    function newHost() {
        if (hostChooser.visible || hostDialog.visible || createDialog.visible || terminalAction.visible) return
        if (spaces.length === 0) return
        if (!hostDefinitions.length) { addHost(); return }
        if (drawer.visible) drawer.close()
        hostChooser.open()
    }
    // A new destination for Band's host list; saving connects to it.
    function addHost() {
        if (hostDialog.visible || createDialog.visible || terminalAction.visible) return
        if (drawer.visible) drawer.close()
        hostName.text = ""; hostDestination.text = ""; hostFolder.text = ""; hostDialog.theme = ""; hostDialog.suggestions = studyHosts.suggestions().filter(alias => !hostDefinitions.some(item => item.destination === alias))
        hostDialog.open()
    }
    function hostOf(appId) {
        if (!appId.startsWith("_host-")) return null
        const id = appId.slice(6).split("_")[0]
        return hostDefinitions.find(host => host.id === id) || {id: id, name: id, destination: "", directory: ""}
    }
    function shellQuote(text) { return "'" + text.replace(/'/g, "'\\''") + "'" }
    function hostCommand(host) {
        let remote = ""
        if (host.directory) {
            const d = host.directory
            const cd = d === "~" ? "cd" : d.startsWith("~/") ? "cd ~/" + shellQuote(d.slice(2)) : "cd " + shellQuote(d)
            remote = " " + shellQuote(cd + " && exec \"$SHELL\" -l")
        }
        return "ssh -t -- " + shellQuote(host.destination) + remote
    }
    // Open panes of a host across workspaces, in strip order.
    function hostPanes(id) {
        const revision = catalogRevision
        let result = []
        for (let i = 0; i < terminalModel.count; ++i) {
            const item = terminalModel.get(i)
            if ((item.appId === "_host-" + id || item.appId.startsWith("_host-" + id + "_")) && !endedApps[item.terminalId]) result.push(i)
        }
        return result.sort((a, b) => (terminalModel.get(a).spaceIndex - terminalModel.get(b).spaceIndex) || (terminalModel.get(a).ordinal - terminalModel.get(b).ordinal))
    }
    function connectHost(id) {
        const host = hostDefinitions.find(item => item.id === id)
        if (!host || spaces.length === 0 || !canCreateTerminal) return
        let n = 1
        while (runningApp(workspace, n === 1 ? "_host-" + id : "_host-" + id + "_" + n) >= 0) n++
        if (drawer.visible) drawer.close()
        studySession.startApp(spaceIds[workspace], {id: n === 1 ? "_host-" + id : "_host-" + id + "_" + n, name: host.name, command: hostCommand(host), directory: ""})
    }
    function openCatalog() {
        if (spaces.length === 0 || appCatalog.visible || terminalAction.visible || createDialog.visible || appearance.visible || commands.visible) return
        if (drawer.visible) drawer.close()
        studyApps.refresh(); appCatalog.open()
    }
    // From the sidebar: go to a pane in any workspace.
    function choosePane(i) { if (drawer.visible) drawer.close(); Qt.callLater(function() { enterPane(i) }) }
    Connections {
        target: studyApps
        function onChecked() {
            const id = window.openAfterCheck; window.openAfterCheck = ""
            if (id && studyApps.entry(id).installed === 1) window.chooseApp(id)
        }
    }
    function isApp(i) { return i >= 0 && i < terminalModel.count && terminalModel.get(i).appId !== "" }
    function runningApp(space, id) {
        const revision = catalogRevision
        for (let i = 0; i < terminalModel.count; ++i) { const item = terminalModel.get(i); if (item.spaceIndex === space && item.appId === id && !endedApps[item.terminalId]) return i }
        return -1
    }
    // Choosing an app already in this workspace goes to its pane; a missing one
    // is installed in a pane of its own (with the chosen recipe, or the first),
    // which opens the app when it succeeds.
    // The welcome: open on a first run, and again from Commands.
    readonly property bool welcomeOpen: welcome.visible
    readonly property bool helpOpen: help.visible
    property var welcomeQueue: []
    function openWelcome() { welcome.step = 0; welcome.picked = ({}); welcome.visible = true; welcome.forceActiveFocus() }
    function finishWelcome(apps, browse) {
        welcome.visible = false; studyWelcome.finish()
        welcomeQueue = apps; openQueuedApp()
        if (browse) openCatalog(); else if (!apps.length) restoreAfterPopup()
    }
    // One app at a time: the session takes one request at a time.
    function openQueuedApp() {
        if (!welcomeQueue.length || studySession.busy) return
        const id = welcomeQueue[0]; welcomeQueue = welcomeQueue.slice(1); chooseApp(id, false)
    }
    Connections { target: studySession; function onChanged() { if (!studySession.busy && window.welcomeQueue.length) Qt.callLater(window.openQueuedApp) } }
    function chooseApp(id, install) {
        const running = runningApp(workspace, id)
        if (running >= 0) { if (drawer.visible) drawer.close(); Qt.callLater(function() { enterPane(running) }); return }
        const entry = studyApps.entry(id)
        if (!entry.id || spaces.length === 0 || !canCreateTerminal) return
        if (entry.installed === 0) {
            const recipe = install || entry.install
            if (!recipe) return
            const installer = id + "-install", pending = runningApp(workspace, installer)
            if (drawer.visible) drawer.close()
            if (pending >= 0) { Qt.callLater(function() { enterPane(pending) }); return }
            let installs = Object.assign({}, pendingInstalls); installs[installer] = id; pendingInstalls = installs
            studySession.startApp(spaceIds[workspace], {id: installer, name: "Install " + entry.name, command: recipe, directory: ""})
            return
        }
        if (drawer.visible) drawer.close()
        studySession.startApp(spaceIds[workspace], {id: entry.id, name: entry.name, command: entry.command, directory: entry.directory || ""})
    }
    // Homebrew's installer runs like an app install; when it finishes the
    // check runs again and the catalog's Sources show the result.
    function installHomebrew() {
        if (spaces.length === 0 || !canCreateTerminal) return
        const pending = runningApp(workspace, "homebrew-install")
        if (pending >= 0) { enterPane(pending); return }
        let installs = Object.assign({}, pendingInstalls); installs["homebrew-install"] = ""; pendingInstalls = installs
        studySession.startApp(spaceIds[workspace], {id: "homebrew-install", name: "Install Homebrew", command: studyApps.homebrew.install, directory: ""})
    }
    // An app's setup from config.conf, again, in a pane of its own.
    function runSetup(id) {
        const entry = studyApps.entry(id)
        if (!entry.setup || spaces.length === 0 || !canCreateTerminal) return
        const pending = runningApp(workspace, id + "-setup")
        if (pending >= 0) { enterPane(pending); return }
        studySession.startApp(spaceIds[workspace], {id: id + "-setup", name: "Set up " + entry.name, command: entry.setup, directory: entry.directory || ""})
    }
    function appFailed(life) { return life.reason === "signal" || life.reason === "startup_failed" || (life.reason === "exited" && life.exit_code !== undefined && life.exit_code !== 0) }
    function appFailure(name, life) {
        return life.reason === "signal" ? name + " was ended by " + life.signal + "."
            : life.reason === "startup_failed" ? name + " could not start."
            : name + " exited with code " + life.exit_code + "." + (life.exit_code === 127 ? " Its command was not found." : "")
    }
    function appEnded(i) {
        const item = terminalModel.get(i), pane = panel(i)
        if (!pane || endedApps[item.terminalId] || appFailed(pane.lifecycle)) return
        const space = item.spaceIndex, items = indices(space), position = items.indexOf(i)
        let ended = Object.assign({}, endedApps); ended[item.terminalId] = true; endedApps = ended
        const installed = pendingInstalls[item.appId]
        if (installed !== undefined) {
            let installs = Object.assign({}, pendingInstalls); delete installs[item.appId]; pendingInstalls = installs
            if (spaceIds[space] === spaceIds[workspace]) { openAfterCheck = installed; studyApps.refresh() }
        }
        ++catalogRevision
        let counted = counts.slice(); counted[space] = Math.max(0, (counted[space] || 1) - 1); counts = counted
        const rest = indices(space), next = rest.length ? rest[Math.max(0, Math.min(rest.length - 1, position))] : -1
        if (remembered[space] === i) { let values = remembered.slice(); values[space] = next; remembered = values }
        // A dialog on top (the app catalog during a setup run, say) keeps the
        // keyboard; the neighbour becomes selected and takes focus when it closes.
        if (selected === i) {
            if (next >= 0 && (choosing || drawer.visible)) { selected = next; let values = remembered.slice(); values[space] = next; remembered = values }
            else if (next >= 0) focusPane(next)
            else { selected = -1; if (!appCatalog.visible && !commands.visible) window.contentItem.forceActiveFocus() }
        }
        Qt.callLater(syncItems)
    }
    function newTerminal() { if (canCreateTerminal && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen) studySession.createTerminal(spaceIds[workspace]) }
    function newWorkspace() {
        if (!canCreateWorkspace || terminalAction.visible || createDialog.visible) return
        workspaceFormMode = "create"; editingWorkspace = ""
        queueWorkspaceForm()
    }
    function editWorkspace(mode) {
        if (studySession.busy || dragSlot >= 0 || terminalAction.visible || createDialog.visible || overview || appearance.visible) return
        workspaceFormMode = mode; editingWorkspace = spaceIds[workspace]
        queueWorkspaceForm()
    }
    function queueWorkspaceForm() {
        pendingWorkspaceDialog = true
        if (drawer.visible) drawer.close()
        else if (appMenu.visible) appMenu.close()
        else { pendingWorkspaceDialog = false; openWorkspaceForm() }
    }
    function openWorkspaceForm() {
        const target = workspaceFormMode === "create" ? workspace : spaceIds.indexOf(editingWorkspace)
        if (target < 0) return
        studySession.clearError()
        newName.text = workspaceFormMode === "create" ? "" : spaces[target]
        newFolder.text = (workspaceFormMode === "create" ? studyConfiguration.workspaceDirectory : "") || directories[target] || studySession.defaultDirectory
        newBranch.text = ""; studyWorktrees.clear()
        if (workspaceFormMode === "create") studyWorktrees.inspect(newFolder.text, studySession.defaultDirectory)
        createDialog.open()
    }
    function browseWorkspaceFolder() {
        if (studySession.busy || !createDialog.visible || workspaceFormMode === "rename") return
        // Qt retains navigation after a dialog's first show. Honor the field on every opening.
        if (folderBrowser) folderBrowser.destroy()
        folderBrowser = folderBrowserFactory.createObject(createDialog, {currentFolder: studySession.folderStart(newFolder.text)})
        if (folderBrowser) folderBrowser.open()
    }
    function restoreFolderFocus() { if (createDialog.visible) newFolder.forceActiveFocus() }
    function submitWorkspaceForm() {
        if (workspaceFormMode === "rename") studySession.renameWorkspace(editingWorkspace, newName.text)
        else if (workspaceFormMode === "folder") studySession.setWorkspaceDirectory(editingWorkspace, newFolder.text)
        // A branch first adds (or finds) its worktree; the workspace opens there.
        else if (newBranch.visible && newBranch.text.trim().length) studyWorktrees.add(newBranch.text)
        else studySession.createWorkspace(newName.text, newFolder.text)
    }
    Connections {
        target: studyWorktrees
        function onAdded(path) {
            if (!createDialog.visible || window.workspaceFormMode !== "create") return
            newFolder.text = path
            studySession.createWorkspace(newName.text.trim() || newBranch.text.trim(), path)
        }
    }
    property bool pendingSearch: false
    readonly property bool terminalSearchOpen: terminalSearch.visible
    readonly property bool terminalSearchFocused: terminalSearch.queryFocused
    function showTerminalSearch() {
        if (overview || selected < 0 || !panel(selected) || !panel(selected).terminal.searchable) return
        reveal()
        const target = panel(selected).terminal
        target.cancelForwarding()
        terminalSearch.controller = target.search
        if (terminalSearch.controller.begin()) terminalSearch.open()
        else terminalSearch.controller = null
    }
    function openTerminalSearch() {
        if (terminalSearch.visible || terminalAction.visible || createDialog.visible || appearance.visible || commands.visible || configurationDialog.visible || help.visible || shortcutSettings.visible || fontSettings.visible || dragSlot >= 0) return
        if (appMenu.visible) { pendingSearch = true; appMenu.close() }
        else showTerminalSearch()
    }
    TerminalSearch {
        id: terminalSearch; objectName: "terminalSearchPopup"; parent: Overlay.overlay
        tones: window.chrome; width: Math.min(560, window.width - window.navigationEdge - 24)
        x: {
            const pane = window.selected >= 0 ? window.panel(window.selected) : null
            const right = pane ? deck.x + pane.x - deck.contentX + pane.width - 8 : window.width - 12
            return Math.max(window.navigationEdge + 12, Math.min(window.width - width - 12, right - width))
        }
        y: 48
        onFinished: Qt.callLater(window.restoreAfterPopup)
    }
    property bool pendingForward: false
    readonly property bool forwardingArmed: selected >= 0 && panel(selected) !== null && panel(selected).terminal.forwardingArmed
    function restoreAfterPopup() {
        if (pendingRuntimeInfo) {
            pendingRuntimeInfo = false
            Qt.callLater(function() { if (identityIndex(runtimeInfoTarget) >= 0) { refreshRuntimeInfo(); terminalInformation.open() } })
            return
        }
        if (pendingAgentInfo) { pendingAgentInfo = false; Qt.callLater(function() { if (identityIndex(agentInfoTarget) >= 0) agentInformation.open() }); return }
        if (pendingSearch) { pendingSearch = false; Qt.callLater(showTerminalSearch); return }
        if (pendingTerminalAction) { pendingTerminalAction = false; Qt.callLater(function() { if (actionExists()) terminalAction.open() }); return }
        if (pendingForward) { pendingForward = false; Qt.callLater(function() { window.dispatchCommand("forward") }); return }
        if (pendingShortcutSettings) {
            pendingShortcutSettings = false; Qt.callLater(function() { studyShortcuts.begin(); shortcutSettings.open() })
        } else if (pendingFontSettings) {
            pendingFontSettings = false; Qt.callLater(function() { studyFonts.begin(); fontSettings.open() })
        } else if (pendingCommands) {
            pendingCommands = false; Qt.callLater(showCommands)
        } else if (pendingAppearance) {
            pendingAppearance = false; Qt.callLater(showAppearance)
        } else if (pendingTerminalColors) {
            pendingTerminalColors = false; Qt.callLater(showTerminalColors)
        } else if (pendingWorkspaceDialog) {
            pendingWorkspaceDialog = false; Qt.callLater(openWorkspaceForm)
        } else if (!overview && selected >= 0 && !welcome.visible && !gallery.visible && !drawer.visible && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen && !appearance.visible && !terminalInformation.visible && !agentInformation.visible && !about.visible && !configurationDialog.visible && !help.visible && !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalSearch.visible && !choosing) panel(selected).terminal.forceActiveFocus()
    }
    Connections {
        target: studySession
        function onCompleted(catalog, method) { window.applyCatalog(catalog, method) }
        function onActionCompleted(identity, method) {
            if (method === "terminal.recover") { const i = window.identityIndex(identity); if (i >= 0) studySession.reconnect(window.panel(i).terminal, identity, false) }
            if (method === "session.recover") {
                for (let i = 0; i < terminalModel.count; ++i) {
                    const pane = window.panel(i)
                    if (pane !== null && pane.lifecycle.state === "stopped") studySession.reconnect(pane.terminal, pane.terminalId, false)
                }
            }
        }
        function onChanged() { if (!studySession.busy && studySession.error.length > 0) window.followMove = false }
    }
    readonly property color accent: chrome.accent
    readonly property int motion: reducedMotion ? 0 : Design.motionMove
    function panel(i) { return panels.itemAt(i) }
    // What a pane runs and where, shared by pane headers and sidebar rows: its
    // kind, the app, agent or foreground program, and its folder or repository.
    function paneIdentity(pane) {
        if (!pane) return {kind: "shell", icon: "shell", name: "", place: "", dirty: false}
        const f = pane.facts
        const host = pane.app ? hostOf(pane.appId) : null
        const kind = host ? "host" : pane.app ? "app" : f.agent ? "agent" : "shell"
        return {kind: kind, icon: kind === "host" ? "host" : kind === "app" ? "apps" : kind === "agent" ? "agents" : "shell",
            name: kind === "host" || kind === "app" ? pane.appName : kind === "agent" ? f.agent : (f.process || ""),
            place: host ? host.destination + (host.directory ? " · " + host.directory : "") : f.title || "Terminal", dirty: !!f.dirty}
    }
    function paneWidth(i) { return Math.max(240, (deck.width + 12) * (fullWidths[i] ? 1 : widths[i]) - 12) }
    function left(i) { if (i < 0 || i >= terminalModel.count) return 0; let x = 0; const items = indices(terminalModel.get(i).spaceIndex); for (let j of items) { if (j === i) break; x += paneWidth(j) + 12 } return x }
    function stripWidth() { const items = indices(workspace); return items.length === 0 ? deck.width : Math.max(deck.width, left(items[items.length - 1]) + paneWidth(items[items.length - 1])) }
    function reveal() {
        if (overview || selected < 0 || selected >= terminalModel.count) return
        deck.directPanning = false
        const x = left(selected), w = paneWidth(selected)
        let target = Math.max(0, Math.min(deck.scrollTarget, stripWidth() - deck.width))
        if (x < target) target = x
        else if (x + w > target + deck.width) target = Math.max(0, x + w - deck.width)
        deck.scrollTarget = target; deck.contentX = target
    }
    function focusPane(i) {
        if (i < 0 || i >= terminalModel.count) return
        selected = i; workspace = terminalModel.get(i).spaceIndex
        let values = remembered.slice(); values[workspace] = i; remembered = values
        // A pane that opens under a chooser (catalog, Commands, hosts) waits for it to close.
        if (!overview) { reveal(); if (!choosing) panel(i).terminal.forceActiveFocus() }
        else Qt.callLater(function() { revealOverview(i) })
    }
    function chooseWorkspace(i) {
        deck.directPanning = false
        let values = positions.slice(); values[workspace] = deck.contentX; positions = values
        workspace = Math.max(0, Math.min(spaces.length - 1, i)); selected = remembered[workspace] === undefined ? -1 : remembered[workspace]
        drawer.close(); deck.scrollTarget = overview ? 0 : Math.max(0, Math.min(positions[workspace] || 0, stripWidth() - deck.width)); deck.contentX = deck.scrollTarget
        // The pane may not exist yet, or any more, by the time this runs.
        if (!overview && selected >= 0) Qt.callLater(function() { const target = panel(selected); if (!target) return; reveal(); target.terminal.forceActiveFocus() })
        else { window.contentItem.forceActiveFocus(); if (overview) deck.contentY = Math.max(0, Math.min(deck.groupTop(workspace), deck.contentHeight - deck.height)) }
    }
    // Drag is client presentation only. Commit one existing service mutation on drop.
    property int dragSlot: -1
    property int dropSpace: -1
    property int dropPosition: -1
    property int dropRaw: -1
    property int dropProjected: -1
    property point dragPoint: Qt.point(0, 0)
    property bool dropAllowed: false
    function cancelDrag() { dragSlot = -1; Qt.callLater(rebuildOverview); dropSpace = -1; dropPosition = -1; dropAllowed = false }
    function updateDrop(point) {
        dragPoint = point; dropSpace = -1; dropPosition = -1; dropAllowed = false
        if (point.x < 0 || point.x > deck.width || point.y < 0 || point.y > deck.height) return
        const y = point.y + deck.contentY
        for (let w = 0; w < spaces.length; ++w) {
            if (overviewFiltered && overviewIndices(w).length === 0) continue
            if (y < deck.groupTop(w) || y >= deck.groupTop(w + 1)) continue
            dropSpace = w
            const row = Math.max(0, Math.floor((y - deck.groupTop(w) - deck.previewTop) / deck.rowStride))
            const cellWidth = deck.cellWidth
            const col = Math.max(0, Math.min(deck.columns - 1, Math.floor(point.x / cellWidth)))
            const items = overviewIndices(w), slot = items[row * deck.columns + col]
            const half = slot === undefined ? cellWidth / 2 : panel(slot).width * panel(slot).previewScale / 2
            const projected = Math.min(items.length, row * deck.columns + col + (point.x - col * cellWidth >= half ? 1 : 0))
            const raw = projected >= items.length ? (items.length ? terminalModel.get(items[items.length - 1]).ordinal + 1 : 0) : terminalModel.get(items[projected]).ordinal
            const source = terminalModel.get(dragSlot)
            dropRaw = raw; dropProjected = projected
            dropPosition = raw - (source.spaceIndex === w && source.ordinal < raw ? 1 : 0)
            dropAllowed = true
            break
        }
    }
    function finishDrag() {
        if (dragSlot >= 0 && dropAllowed) {
            const source = terminalModel.get(dragSlot)
            if (source.spaceIndex !== dropSpace || source.ordinal !== dropPosition)
                studySession.moveTerminal(source.terminalId, spaceIds[dropSpace], dropPosition)
        }
        cancelDrag()
    }
    function toggleOverview() {
        cancelDrag()
        if (!overview) { let values = positions.slice(); values[workspace] = deck.contentX; positions = values }
        overview = !overview; deck.scrollTarget = overview ? 0 : Math.max(0, Math.min(positions[workspace] || 0, stripWidth() - deck.width)); deck.contentX = deck.scrollTarget; deck.contentY = 0
        if (overview) { window.contentItem.forceActiveFocus(); if (selected >= 0) Qt.callLater(function() { revealOverview(selected) }) }
        else if (selected >= 0) Qt.callLater(function() { reveal(); panel(selected).terminal.forceActiveFocus() })
    }
    function enterPane(i) { focusPane(i); if (overview) toggleOverview() }
    function navigate(delta) {
        const items = overview ? overviewIndices(workspace) : indices(workspace); if (items.length) focusPane(items[Math.max(0, Math.min(items.length - 1, items.indexOf(selected) + delta))])
    }
    readonly property bool moveKeyContext: !overviewSearch.activeFocus && !navigation.searchFocused && !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalAction.visible && !createDialog.visible && !appearance.visible && !terminalInformation.visible && !agentInformation.visible && !about.visible && !configurationDialog.visible && !help.visible && !drawer.visible && !appMenu.visible && !paneMenu.visible && dragSlot < 0
    property string moveNotice: ""
    property bool followMove: false
    function canReorder(delta) {
        if (selected < 0 || studySession.busy || (overview && overviewOrdinals[selected] < 0)) return false
        const items = indices(terminalModel.get(selected).spaceIndex), position = items.indexOf(selected) + delta
        return position >= 0 && position < items.length
    }
    function reorderSelected(delta) {
        if (!canReorder(delta)) return
        moveNotice = ""
        const source = terminalModel.get(selected), items = indices(source.spaceIndex)
        // Catalog positions include ended apps; move next to the neighbour in the strip.
        const neighbour = terminalModel.get(items[items.indexOf(selected) + delta])
        followMove = true
        studySession.moveTerminal(source.terminalId, spaceIds[source.spaceIndex], neighbour.ordinal)
    }
    function canMoveWorkspace(delta) {
        const target = workspace + delta
        // An app stays in its workspace.
        return selected >= 0 && !isApp(selected) && (!overview || overviewOrdinals[selected] >= 0) && !studySession.busy && target >= 0 && target < spaces.length
    }
    function moveSelectedWorkspace(delta) {
        if (selected < 0 || isApp(selected) || studySession.busy || (overview && overviewOrdinals[selected] < 0)) return
        const target = workspace + delta
        if (target < 0 || target >= spaces.length) return
        moveNotice = ""
        followMove = true
        studySession.moveTerminal(terminalModel.get(selected).terminalId, spaceIds[target], -1)
    }
    function revealOverview(i) {
        const source = terminalModel.get(i)
        if (overviewOrdinals[i] < 0) return
        const top = deck.groupTop(source.spaceIndex) + Math.floor(overviewOrdinals[i] / deck.columns) * deck.rowStride + deck.previewTop
        if (top < deck.contentY) deck.contentY = top
        else if (top + deck.previewHeight + 52 > deck.contentY + deck.height) deck.contentY = top + deck.previewHeight + 52 - deck.height
        deck.contentY = Math.max(0, Math.min(deck.contentHeight - deck.height, deck.contentY))
    }
    function cycleWidth() {
        if (selected < 0) return
        let values = widths.slice(), full = fullWidths.slice()
        values[selected] = values[selected] === 0.5 ? 2/3 : values[selected] > 0.6 ? 1/3 : 0.5
        full[selected] = false; widths = values; fullWidths = full; Qt.callLater(reveal)
    }
    function toggleFull() {
        if (selected < 0) return
        let values = fullWidths.slice(); values[selected] = !values[selected]; fullWidths = values; Qt.callLater(reveal)
    }
    component QuietButton: ChromeButton { tones: window.chrome }
    component MenuAction: ChromeMenuItem { tones: window.chrome }
    component MenuRule: MenuSeparator {
        topPadding: Design.spaceXs; bottomPadding: Design.spaceXs; leftPadding: Design.spaceM; rightPadding: Design.spaceM
        contentItem: Rectangle { implicitWidth: 248; implicitHeight: 1; color: window.chrome.border }
    }
    Rectangle { x: 0; y: 0; width: parent.width; height: 40; color: window.chrome.sidebar }
    Rectangle { x: 0; y: 40; width: 44; height: parent.height - 40; color: window.chrome.sidebar }
    // With an integrated title bar, empty top bar space moves the window and a
    // double click maximizes it; controls above it keep their own input.
    Item {
        objectName: "titleDragArea"; x: 0; y: 0; width: parent.width; height: 40; enabled: window.titleIntegrated
        // Linux: the compositor moves the window.
        DragHandler { target: null; enabled: window.ownTitle; onActiveChanged: if (active) window.startSystemMove() }
        TapHandler { enabled: window.ownTitle; onDoubleTapped: window.toggleMaximized() }
        // macOS: Band moves the window itself; a system move does not start from
        // a folded-in title bar there. Double click zooms, as on a title bar.
        MouseArea {
            anchors.fill: parent; enabled: window.appleTitle
            property point grab
            onPressed: mouse => grab = Qt.point(mouse.x, mouse.y)
            onPositionChanged: mouse => { if (pressed && !window.maximized) { window.x += mouse.x - grab.x; window.y += mouse.y - grab.y } }
            onDoubleClicked: window.toggleMaximized()
        }
    }
    Row {
        id: windowButtons; objectName: "windowButtons"; visible: window.ownTitle
        anchors.right: parent.right; anchors.rightMargin: Design.spaceS; y: 6; spacing: Design.space2xs
        QuietButton { objectName: "windowMinimize"; height: 28; iconName: "minimize"; hint: "Minimize"; focusPolicy: Qt.NoFocus; onClicked: window.showMinimized() }
        QuietButton { objectName: "windowMaximize"; height: 28; iconName: window.maximized ? "restore" : "maximize"; hint: window.maximized ? "Restore" : "Maximize"; focusPolicy: Qt.NoFocus; onClicked: window.toggleMaximized() }
        QuietButton { objectName: "windowClose"; height: 28; iconName: "close"; hint: "Close window"; focusPolicy: Qt.NoFocus; onClicked: window.close() }
    }
    QuietButton { objectName: "applicationMenuButton"; visible: !window.systemMenus; x: 8 + window.titleInset; y: 6; height: 28; iconName: "menu"; hint: "Application menu"; selected: appMenu.visible; onClicked: appMenu.open() }
    Column {
        x: 8; y: 52; spacing: Design.spaceXl
        // All panes clears the filters below it (or closes the sidebar when
        // nothing is filtered); each kind icon filters the workspaces' panes.
        QuietButton {
            objectName: "workspacesButton"; height: 28; iconName: window.navigationKinds.length ? "filter-off" : "workspaces"
            hint: window.navigationKinds.length ? "All panes · clear filters" : window.navigationShown ? "Hide the sidebar" : "Show all panes"
            selected: window.navigationShown && !window.navigationKinds.length; onClicked: window.showAllPanes()
        }
        Rectangle { x: 6; width: 16; height: Design.line; color: window.chrome.border }
        QuietButton {
            objectName: "shellsButton"; height: 28; iconName: "shell"; hint: "Shells"
            selected: window.navigationShown && window.navigationKinds.indexOf("shells") >= 0
            onClicked: window.toggleKind("shells")
        }
        QuietButton {
            objectName: "appsButton"; height: 28; iconName: "apps"; hint: "Apps"
            selected: window.navigationShown && window.navigationKinds.indexOf("apps") >= 0
            onClicked: window.toggleKind("apps")
        }
        QuietButton {
            objectName: "hostsButton"; height: 28; iconName: "host"; hint: "Hosts"
            selected: window.navigationShown && window.navigationKinds.indexOf("hosts") >= 0
            onClicked: window.toggleKind("hosts")
        }
        QuietButton {
            objectName: "agentsButton"; height: 28; iconName: "agents"
            hint: window.waitingCount ? "Agents · " + window.waitingCount + " need input (last reports)" : "Agents"
            selected: window.navigationShown && window.navigationKinds.indexOf("agents") >= 0; onClicked: window.toggleKind("agents")
            Rectangle {
                objectName: "agentsBadge"; x: 19; y: -7; width: 16; height: 16; radius: 8; visible: window.waitingCount > 0; color: window.chrome.waiting
                Text { anchors.centerIn: parent; text: window.waitingCount; color: window.chrome.background; font.pixelSize: Design.textCaption }
            }
        }
    }
    // The pin sits at the foot of the rail; Keyboard shortcuts stay in the menu,
    // the command menu and their own chord.
    QuietButton {
        objectName: "navigationPin"; x: 8; height: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: Design.spaceL
        iconName: "pin"; selected: window.navigationPinned
        enabled: window.width >= 920 || window.navigationPinned
        hint: window.navigationPinned ? "Unpin navigation" : window.width < 920 ? "Widen the window to pin navigation" : "Pin navigation"
        onClicked: window.toggleNavigationPin()
    }
    TopBar {
        id: topbar; x: Math.max(window.navigationEdge, window.titleInset + 44); y: 4; width: parent.width - x - 12 - (windowButtons.visible ? windowButtons.width + 12 : 0)
        tones: window.chrome; accent: window.accent; overview: window.overview; canCreate: window.canCreateTerminal
        workspaceName: window.overview ? "Overview" : (window.spaces[window.workspace] || "No workspaces")
        readonly property var paneIndices: window.indices(window.workspace)
        focusPosition: paneIndices.indexOf(window.selected)
        markers: paneIndices.map(i => {
            const item = window.panel(i)
            return {title: item ? item.facts.title : "Terminal", selected: window.selected === i, waiting: item !== null && !!item.facts.needs_input}
        })
        inView: position => { const i = paneIndices[position]; return i !== undefined && window.left(i) + window.paneWidth(i) > deck.contentX && window.left(i) < deck.contentX + deck.width }
        onWorkspaceClicked: window.openNavigation([])   // every workspace, no kind filter
        onMarkerClicked: position => window.enterPane(paneIndices[position])
        onNewTerminalClicked: window.newTerminal()
        onOverviewClicked: window.toggleOverview()
    }
    Flow {
        id: overviewFilters; x: window.navigationEdge + 16; y: 54; width: window.width - x - 12; spacing: Design.spaceM; visible: window.overview
        ContextSearch {
            id: overviewSearch; objectName: "overviewSearch"; tones: window.chrome
            width: Math.min(240, overviewFilters.width); placeholderText: "Find terminals…"; Accessible.name: "Find terminals by workspace, folder, Git or agent"
            onTextChanged: window.overviewQuery = text
            onEscapePressed: { if (text.length) clear(); else window.toggleOverview() }
            onStepRequested: direction => {
                let results = []; for (const group of window.overviewGroups) results = results.concat(group)
                if (results.length) { const current = results.indexOf(window.selected); const next = current < 0 ? (direction > 0 ? 0 : results.length - 1) : Math.max(0, Math.min(results.length - 1, current + direction)); window.focusPane(results[next]); forceActiveFocus() }
            }
            onAccepted: { if (!inputMethodComposing) { const groups = window.overviewGroups; const found = groups.reduce((all, group) => all.concat(group), []); const i = found.indexOf(window.selected) >= 0 ? window.selected : found[0]; if (i !== undefined) window.enterPane(i) } }
        }
        QuietButton { objectName: "overviewAgentFilter"; hint: "Filter terminal or agent identity"; text: window.agentsOnly ? "Show: Agents" : "Show: All terminals"; onClicked: window.changeFilter(!window.agentsOnly, window.needsOnly); background: Rectangle { radius: Design.radiusM; color: window.agentsOnly ? window.chrome.surface : "transparent"; border.width: window.agentsOnly ? Design.line : 0; border.color: window.accent } }
        QuietButton { objectName: "overviewStateFilter"; hint: "Filter agents that need input"; text: window.needsOnly ? "State: Needs input" : "State: All states"; onClicked: window.changeFilter(window.agentsOnly, !window.needsOnly); background: Rectangle { radius: Design.radiusM; color: window.needsOnly ? window.chrome.surface : "transparent"; border.width: window.needsOnly ? Design.line : 0; border.color: window.accent } }
        QuietButton { objectName: "overviewClearFilters"; text: "Clear filters"; visible: window.overviewFiltered; onClicked: window.clearOverviewFilters() }
        Label { height: 32; verticalAlignment: Text.AlignVCenter; text: window.overviewMatches + (window.overviewMatches === 1 ? " match" : " matches"); color: window.chrome.muted; font.pixelSize: Design.textSmall }
    }
    Flickable {
        id: deck; objectName: "deck"
        onContentXChanged: window.scheduleViewSave()
        onContentYChanged: window.scheduleViewSave()
        property real scrollTarget: 0
        property bool directPanning: false
        x: window.navigationEdge + 16; y: window.overview ? overviewFilters.y + overviewFilters.implicitHeight + 16 : 60; width: parent.width - x - 12; height: parent.height - y - 12
        clip: true; boundsBehavior: Flickable.StopAtBounds
        interactive: window.overview && window.dragSlot < 0
        contentWidth: window.overview ? width : window.stripWidth()
        contentHeight: window.overview ? overviewHeight() : height
        readonly property real previewHeight: columns < 3 ? Math.max(150, Math.min(columns === 1 ? 460 : 340, height - 110)) : Math.min(240, Math.max(150, (height - 100) / 2))
        readonly property int columns: Math.max(1, Math.min(3, Math.floor((width + 16) / 296)))
        readonly property real cellWidth: (width + 16) / columns
        readonly property real cardWidth: cellWidth - 22
        readonly property real previewTop: 66
        readonly property real rowStride: previewHeight + 64
        function rows(space) { return Math.max(1, Math.ceil(window.overviewIndices(space).length / columns)) }
        function groupTop(space) { let y = 0; for (let i = 0; i < space; ++i) if (!window.overviewFiltered || window.overviewIndices(i).length) y += window.overviewIndices(i).length ? rows(i) * rowStride + 76 : 152; return y }
        function overviewHeight() { return Math.max(height, groupTop(window.spaces.length)) }
        Behavior on contentX { enabled: !window.overview && !deck.directPanning; NumberAnimation { id: stripAnimation; duration: window.motion; easing.type: Easing.OutCubic } }
        // Keep the gesture receiver fixed while terminal panes move out of view.
        WheelHandler {
            orientation: Qt.Horizontal
            blocking: false
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            target: null
            onWheel: event => {
                const precise = event.pixelDelta.x !== 0 || event.pixelDelta.y !== 0
                const dx = precise ? event.pixelDelta.x : event.angleDelta.x
                const dy = precise ? event.pixelDelta.y : event.angleDelta.y
                if (!window.overview && dx !== 0 && Math.abs(dx) > Math.abs(dy)) {
                    // Pixel deltas are direct manipulation; easing each event loses motion.
                    // Coarse wheel events accumulate against the destination while easing.
                    const origin = precise && !deck.directPanning ? deck.contentX : deck.scrollTarget
                    deck.directPanning = precise
                    if (precise) stripAnimation.stop()
                    deck.scrollTarget = Math.max(0, Math.min(deck.contentWidth - deck.width, origin - dx))
                    deck.contentX = deck.scrollTarget; event.accepted = true
                } else event.accepted = false
            }
        }
        Repeater {
            model: window.spaces.length
            Item {
                required property int index
                x: 0; y: deck.groupTop(index); width: deck.width; height: 52
                visible: window.overview && (!window.overviewFiltered || window.overviewIndices(index).length > 0)
                Label { x: 0; y: 0; width: parent.width - 100; text: window.spaces[index]; textFormat: Text.PlainText; elide: Text.ElideRight; color: window.chrome.foreground; font.pixelSize: Design.textHeading; font.weight: Font.Medium }
                Label { anchors.right: parent.right; y: 3; text: window.overviewIndices(index).length + (window.overviewIndices(index).length === 1 ? " terminal" : " terminals"); color: window.chrome.muted; font.pixelSize: Design.textSmall }
                Label { x: 0; y: 25; width: parent.width; text: window.directories[index] || ""; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: window.chrome.muted; font.pixelSize: Design.textSmall }
                Label { x: 0; y: deck.previewTop + 6; visible: window.overviewIndices(index).length === 0; text: window.dragSlot >= 0 && window.dropSpace === index ? "Drop terminal here" : "No terminals yet"; leftPadding: window.dragSlot >= 0 && window.dropSpace === index ? 14 : 0; color: window.chrome.muted; font.pixelSize: Design.textBody }
                Rectangle { visible: index > 0; x: 0; y: -18; width: parent.width; height: 1; color: window.chrome.border }
            }
        }
        // Unscaled outlines keep focus and pointer feedback crisp around previews.
        Repeater {
            model: terminalModel
            Rectangle {
                required property int index
                readonly property var pane: { const revision = window.catalogRevision; const ready = window.terminalItems.length; return window.panel(index) }
                visible: window.overview && pane !== null && pane.visible
                x: pane ? pane.x - 3 : 0; y: pane ? pane.y - 3 : 0
                width: pane ? pane.width * pane.scale + 6 : 0; height: pane ? pane.height * pane.scale + 6 : 0
                color: "transparent"; radius: Design.radiusL
                border.width: window.selected === index ? Design.lineStrong : Design.line
                border.color: pane ? window.selected === index || pane.previewHovered ? studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).accent : window.chrome.border : window.chrome.border
            }
        }
        Repeater {
            id: panels; model: terminalModel
            delegate: Rectangle {
                id: pane; required property int index
                required property int spaceIndex
                required property int ordinal
                required property string terminalId
                required property string appId
                required property string appName
                readonly property bool app: appId !== ""
                readonly property bool ended: window.endedApps[terminalId] === true
                Connections { target: pane.runtimeFacts; function onChanged() { Qt.callLater(window.rebuildOverview) } }
                property alias terminal: terminal
                readonly property var runtimeFacts: studySession.observe(terminalId)
                readonly property var facts: runtimeFacts ? runtimeFacts.facts : ({title: "Terminal", agent: "", state: ""})
                opacity: window.dragSlot === index ? 0.45 : 1
                readonly property int overviewOrdinal: window.overviewOrdinals[index] ?? -1
                readonly property real previewScale: Math.min(deck.cardWidth / width, deck.previewHeight / height)
                objectName: "pane" + index
                width: window.paneWidth(index); height: window.height - 72
                x: window.overview ? 3 + (overviewOrdinal % deck.columns) * deck.cellWidth : window.left(index)
                y: window.overview ? deck.groupTop(spaceIndex) + Math.floor(overviewOrdinal / deck.columns) * deck.rowStride + deck.previewTop : 0
                scale: window.overview ? previewScale : 1
                transformOrigin: Item.TopLeft
                visible: !ended && (window.overview ? overviewOrdinal >= 0 : spaceIndex === window.workspace)
                // One panel in the workspace's terminal background with a hairline
                // (after Rex): the header blends into it and no frame marks focus;
                // the header's name, the cursor and the sidebar show which pane has
                // it. A terminal with its own colors shows them inside the panel.
                color: studyThemes.theme(window.themeIndexFor(spaceIndex)).background; radius: Design.radiusL
                border.width: window.overview ? 0 : Design.line; border.color: studyThemes.theme(window.themeIndexFor(spaceIndex)).border
                Behavior on x { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                // Background chrome focuses the reader; terminal and menu controls
                // above it keep handling their own pointer events.
                MouseArea {
                    anchors.fill: parent; enabled: !window.overview
                    onClicked: window.focusPane(pane.index)
                }
                readonly property var identity: window.paneIdentity(pane)
                readonly property var tones: studyThemes.theme(window.themeIndexFor(spaceIndex))
                PaneHeader {
                    width: parent.width; tones: pane.tones; terminalId: pane.terminalId
                    icon: pane.identity.icon; stopped: pane.appStopped; name: pane.identity.name; place: pane.identity.place; dirty: pane.identity.dirty
                    ownTheme: appearance.visible && window.appearanceTerminal === pane.terminalId ? (window.previewDefault ? -1 : window.previewTheme) : window.ownTheme(pane.terminalId)
                    agentState: pane.facts.agent ? pane.facts.state : ""; needsInput: !!pane.facts.needs_input
                    focused: window.selected === pane.index && !window.overview; fullWidth: !!window.fullWidths[pane.index]
                    exited: window.exitedTerminal(pane.terminalId); busy: studySession.busy
                    menuEnabled: !window.overview; colorsEnabled: !window.overview
                    onWidthClicked: window.dispatchCommand("cycle-width")
                    onFullClicked: window.dispatchCommand("full-width")
                    onCloseClicked: window.closePane(pane.terminalId)
                    onColorsClicked: window.openTerminalColors(pane.terminalId)
                    onMenuClicked: anchor => { window.focusPane(pane.index); window.paneMenuTarget = pane.terminalId; const point = anchor.mapToItem(window.contentItem, 0, anchor.height); paneMenu.x = Math.max(0, Math.min(point.x, window.width - paneMenu.width - 12)); paneMenu.y = point.y; paneMenu.open() }
                }
                TerminalItem {
                    id: terminal; objectName: "terminal" + index
                    property int workspaceIndex: pane.spaceIndex
                    property int orderInWorkspace: pane.ordinal
                    property string stableIdentity: pane.terminalId
                    property var runtimeFacts: pane.facts
                    x: 4; y: 36; width: parent.width - 8; height: parent.height - 40
                    enabled: !window.overview
                    textFont: studyFonts.current
                    themeIndex: window.terminalThemeIndex(pane.terminalId, pane.spaceIndex)
                    presentationEnabled: pane.visible && pane.x + pane.width * pane.scale > deck.contentX && pane.x < deck.contentX + deck.width && pane.y + pane.height * pane.scale > deck.contentY && pane.y < deck.contentY + deck.height
                    opacity: presentationEnabled ? 1 : 0
                    Accessible.role: Accessible.Terminal; Accessible.name: pane.facts.title + " in " + window.spaces[pane.spaceIndex]
                    onStatusChanged: {
                        if (!ready && attachment !== "connecting") studySession.inspect(pane.terminalId)
                        else if (ready && window.selected === pane.index) Qt.callLater(window.restoreAfterPopup)
                    }
                    Component.onCompleted: {
                        studySession.attach(terminal, pane.terminalId)
                        let items = window.terminalItems.slice(); items.push(terminal); window.terminalItems = items
                    }
                    onActiveFocusChanged: if (activeFocus && !window.overview) window.focusPane(index)
                }
                // Unfocused panes recede a little: a veil of the terminal's own
                // background, so its background is unchanged and text loses some contrast.
                // dim-unfocused-panes = false in config.conf turns it off.
                Rectangle {
                    objectName: "paneDim"; anchors.fill: terminal; visible: opacity > 0
                    color: studyThemes.theme(terminal.themeIndex).background
                    opacity: studyConfiguration.dimUnfocused && !window.overview && window.selected >= 0 && window.selected !== pane.index ? 0.18 : 0
                }
                readonly property var lifecycle: studySession.lifecycle[terminalId] || ({state: "unavailable", message: "Checking terminal status…", busy: false})
                onLifecycleChanged: if (app && lifecycle.state === "stopped" && !lifecycle.busy) window.appEnded(index)
                readonly property bool appStopped: app && lifecycle.state === "stopped"
                Rectangle {
                    anchors.centerIn: terminal; width: Math.min(parent.width - 40, pane.appStopped ? 560 : 420); height: lifecycleBody.implicitHeight + 32; radius: Design.radiusXl
                    visible: !terminal.ready && !(terminalSearch.visible && window.selected === pane.index && terminal.searchable); color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).raised; border.color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).border
                    Column {
                        id: lifecycleBody; x: 16; y: 16; width: parent.width - 32; spacing: Design.spaceL
                        Label {
                            width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: Design.textControl
                            color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).foreground
                            objectName: "terminalLifecycleMessage" + pane.index
                            text: terminal.attachment === "connecting" ? "Connecting…" : pane.appStopped ? window.appFailure(pane.appName, pane.lifecycle) : pane.lifecycle.state === "stopped" ? pane.lifecycle.message : terminal.attachment === "elsewhere" ? "Open in another window" : pane.lifecycle.state === "running" ? "Could not attach: " + terminal.status : (pane.lifecycle.message || "Checking terminal status…")
                        }
                        Label {
                            width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: Design.textSmall
                            color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).muted
                            text: pane.appStopped ? (pane.lifecycle.output ? "Its last lines are below. " : "") + (window.hostOf(pane.appId) ? "Check the host in config.conf or your ssh settings, then connect again." : "Fix its command in config.conf, then start it again.") : pane.lifecycle.state === "stopped" ? "Start a new shell in this workspace’s folder. The previous process has ended." : terminal.attachment === "elsewhere" ? "Move this terminal here to continue with the same process." : "Reconnect keeps a running shell. Check again if its status has changed."
                            visible: terminal.attachment !== "connecting"
                        }
                        Rectangle {
                            width: parent.width; height: Math.min(appOutput.implicitHeight + 16, pane.height * 0.4); radius: Design.radiusM; clip: true
                            visible: pane.appStopped && (pane.lifecycle.output || "").length > 0
                            color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).background; border.color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).border
                            TextEdit {
                                id: appOutput; objectName: "appOutput" + pane.index; x: 8; y: 8; width: parent.width - 16
                                readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap
                                text: pane.lifecycle.output || ""; color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).foreground
                                font.family: studyFonts.current.family; font.pointSize: 11
                            }
                        }
                        Flow {
                            width: parent.width; spacing: Design.spaceM; visible: terminal.attachment !== "connecting"
                            QuietButton {
                                objectName: "terminalPrimary" + pane.index; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex))
                                text: pane.appStopped ? (window.hostOf(pane.appId) ? "Connect again" : "Start again") : pane.lifecycle.state === "stopped" ? "Start new shell" : terminal.attachment === "elsewhere" ? "Move here…" : "Reconnect"
                                visible: pane.lifecycle.state === "stopped" || pane.lifecycle.state === "running"
                                enabled: !studySession.busy && !pane.lifecycle.busy && !window.overview
                                onClicked: { if (pane.lifecycle.state === "stopped") studySession.recover(pane.terminalId); else if (terminal.attachment === "elsewhere") window.requestTerminalAction(pane.terminalId, "move"); else studySession.reconnect(terminal, pane.terminalId, false) }
                            }
                            QuietButton { objectName: "terminalReconnect" + pane.index; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)); text: "Reconnect"; visible: terminal.attachment === "elsewhere" && pane.lifecycle.state === "running"; enabled: !studySession.busy && !pane.lifecycle.busy && !window.overview; onClicked: studySession.reconnect(terminal, pane.terminalId, false) }
                            QuietButton { objectName: "terminalCloseExited" + pane.index; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)); text: "Close pane"; visible: pane.lifecycle.state === "stopped"; enabled: !studySession.busy && !pane.lifecycle.busy && !window.overview; onClicked: studySession.dismissTerminal(pane.terminalId) }
                            QuietButton { objectName: "terminalCheck" + pane.index; visible: !pane.appStopped; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)); text: pane.lifecycle.busy ? "Checking…" : "Check again"; enabled: !pane.lifecycle.busy && !window.overview; onClicked: studySession.inspect(pane.terminalId) }
                        }
                    }
                }
                readonly property bool previewHovered: previewPointer.containsMouse
                MouseArea {
                    id: previewPointer; hoverEnabled: true
                    anchors.fill: parent; enabled: window.overview && !studySession.busy
                    cursorShape: window.dragSlot === index ? Qt.ClosedHandCursor : Qt.ArrowCursor
                    preventStealing: true
                    property point pressedAt
                    property bool moved: false
                    onPressed: mouse => { pressedAt = mapToItem(deck, mouse.x, mouse.y); moved = false }
                    onPositionChanged: mouse => {
                        if (!pressed) return
                        const point = mapToItem(deck, mouse.x, mouse.y)
                        if (!moved && Math.abs(point.x - pressedAt.x) + Math.abs(point.y - pressedAt.y) >= Qt.styleHints.startDragDistance) {
                            moved = true; window.dragSlot = index
                        }
                        if (moved && window.dragSlot === index) window.updateDrop(point)
                    }
                    onReleased: { if (moved) window.finishDrag(); else window.enterPane(index) }
                    onCanceled: window.cancelDrag()
                }
            }
        }
        Repeater {
            id: captions; model: terminalModel
            Item {
                id: caption
                required property int index
                objectName: "overviewCaption" + index
                Component.onCompleted: { let labels = window.overviewLabels.slice(); labels[index] = this; window.overviewLabels = labels }
                readonly property var pane: { const revision = window.catalogRevision; const ready = window.terminalItems.length; return panels.itemAt(index) }
                visible: window.overview && pane !== null && pane.visible
                x: pane ? pane.x : 0; y: pane ? pane.y + pane.height * pane.scale + 10 : 0
                width: deck.cardWidth; height: 42
                readonly property string text: pane ? pane.facts.title + (pane.facts.agent ? " · " + pane.facts.agent + " · " + pane.facts.state : "") : ""
                Accessible.name: text
                TapHandler { onTapped: window.enterPane(caption.index) }
                Column {
                    anchors.fill: parent; spacing: Design.spaceXs
                    Text { width: parent.width; text: caption.pane ? caption.pane.facts.title : ""; textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: Design.textBody; color: window.selected === caption.index ? window.chrome.foreground : window.chrome.muted }
                    Text { width: parent.width; text: caption.pane && caption.pane.facts.agent ? caption.pane.facts.agent + " · " + caption.pane.facts.state : ""; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: Design.textSmall; color: caption.pane && caption.pane.facts.needs_input ? window.chrome.waiting : window.chrome.muted }
                }
            }
        }
        Label { anchors.centerIn: parent; visible: window.overview && window.spaces.length > 0 && window.overviewFiltered && window.overviewMatches === 0; text: window.overviewQuery.trim().length ? "No matching terminals" : window.needsOnly ? "No agents need input" : "No agents detected"; color: window.chrome.muted; font.pixelSize: Design.textControl }
        Label { anchors.centerIn: parent; visible: window.spaces.length === 0 || (!window.overview && (window.counts[window.workspace] || 0) === 0); text: window.spaces.length ? "An empty workspace" : "No workspaces yet"; color: window.chrome.muted; font.pixelSize: Design.textControl }
    }
    Item {
        x: deck.x; y: deck.y; width: deck.width; height: deck.height; clip: true
        Rectangle {
            visible: window.dragSlot >= 0 && window.dropSpace >= 0
            x: 0; y: deck.groupTop(window.dropSpace) - deck.contentY
            width: deck.width; height: deck.groupTop(window.dropSpace + 1) - deck.groupTop(window.dropSpace) - 8
            color: "transparent"; radius: Design.radiusL; border.width: Design.line
            border.color: window.dropAllowed ? window.accent : window.chrome.waiting
        }
        Rectangle {
            visible: window.dragSlot >= 0 && window.dropSpace >= 0 && window.dropAllowed
            readonly property int total: window.overviewIndices(window.dropSpace).length
            readonly property int before: Math.min(window.dropProjected, Math.max(0, total - 1))
            readonly property int slot: window.dropSpace < 0 ? -1 : (window.overviewIndices(window.dropSpace)[before] ?? -1)
            x: slot < 0 ? 6 : (before % deck.columns) * deck.cellWidth + (window.dropProjected === total ? window.panel(slot).width * window.panel(slot).previewScale + 5 : -5)
            y: deck.groupTop(window.dropSpace) + Math.floor(before / deck.columns) * deck.rowStride + deck.previewTop - deck.contentY
            width: 3; height: window.dropSpace >= 0 && window.overviewIndices(window.dropSpace).length === 0 ? 56 : deck.previewHeight; radius: 1.5; color: window.accent
        }
    }
    Rectangle {
        visible: window.dragSlot >= 0
        x: Math.min(window.width - width - 12, deck.x + window.dragPoint.x + 16)
        y: Math.max(60, Math.min(window.height - height - 12, deck.y + window.dragPoint.y + 16))
        width: dragLabel.implicitWidth + 24; height: 38; radius: Design.radiusL; color: window.chrome.surface; border.color: window.accent; z: 5
        Label { id: dragLabel; anchors.centerIn: parent; text: (window.dropSpace < 0 ? "Move terminal" : window.spaces[window.dropSpace] + " · position " + (window.dropPosition + 1)) + " · Esc cancels"; color: window.chrome.foreground }
    }
    Timer {
        interval: 16; repeat: true; running: window.dragSlot >= 0
        onTriggered: {
            const y = window.dragPoint.y
            const delta = y < 56 ? -Math.min(12, (56 - y) / 5) : y > deck.height - 56 ? Math.min(12, (y - deck.height + 56) / 5) : 0
            if (delta && window.dragPoint.x >= 0 && window.dragPoint.x <= deck.width) { deck.contentY = Math.max(0, Math.min(deck.contentHeight - deck.height, deck.contentY + delta)); window.updateDrop(window.dragPoint) }
        }
    }
    Rectangle {
        id: pinnedNavigation; x: 44; y: 40; width: 244; height: window.height - 40
        visible: window.navigationDocked; color: window.chrome.sidebar
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: window.chrome.border }
    }
    Welcome {
        id: welcome; objectName: "welcome"; visible: false; z: 40
        x: window.navigationEdge; y: 0; width: parent.width - x; height: parent.height
        host: window; tones: window.chrome
        onFinished: (apps, browse) => window.finishWelcome(apps, browse)
    }
    Gallery {
        id: gallery; objectName: "gallery"; visible: false; z: 40
        x: window.navigationEdge; y: 0; width: parent.width - x; height: parent.height
        host: window; tones: window.chrome
        onClosed: { visible = false; window.contentItem.forceActiveFocus() }
    }
    NavigationPanel {
        id: navigation
        parent: window.navigationDocked ? pinnedNavigation : drawer.contentItem
        anchors.fill: parent
        host: window; session: studySession
    }
    // The unpinned panel opens beside the rail rather than over it (a Drawer
    // always starts at the window edge), so rail icons stay usable while it
    // is open. Like the modal drawer it replaces, a press elsewhere only closes it.
    MouseArea {
        id: navigationShade; objectName: "navigationShade"; anchors.fill: parent; z: 50; visible: drawer.visible
        acceptedButtons: Qt.AllButtons
        onPressed: mouse => { if (mouse.x < 44 && mouse.y >= 40) mouse.accepted = false; else drawer.close() }
        onWheel: wheel => wheel.accepted = true
        Rectangle { width: parent.width; height: 40; color: "#000000"; opacity: 0.5 * drawer.opacity }
        Rectangle { x: 44; y: 40; width: parent.width - 44; height: parent.height - 40; color: "#000000"; opacity: 0.5 * drawer.opacity }
    }
    FocusScope {
        id: drawer; objectName: "navigationDrawer"; x: 44; y: 40; z: 51; width: 244; height: window.height - 40; visible: false; opacity: 0
        property alias contentItem: drawerContent
        signal opened()
        signal closed()
        function open() {
            if (visible) return
            visible = true; opacity = 1; forceActiveFocus(Qt.OtherFocusReason); opened()
        }
        function close() {
            if (!visible) return
            visible = false; opacity = 0; closed()
        }
        Behavior on opacity { enabled: !window.reducedMotion && drawer.visible; NumberAnimation { duration: Design.motionQuick } }
        Keys.onEscapePressed: close()
        Rectangle { anchors.fill: parent; color: window.chrome.sidebar; border.color: window.chrome.border }
        Item { id: drawerContent; anchors.fill: parent }
        onOpened: window.focusNavigation()
        onClosed: window.restoreAfterPopup()
    }
    ChromeMenu {
        tones: window.chrome
        onClosed: window.restoreAfterPopup()
        id: appMenu; x: 8; y: 40
        MenuAction { objectName: "commandsMenuItem"; keyHint: studyShortcuts.labels["commands"] || ""; text: "Open Commands"; onTriggered: window.openCommands() }
        MenuRule {}
        MenuAction { keyHint: studyShortcuts.labels["new-terminal"] || ""; text: "New terminal"; enabled: window.canCreateTerminal; onTriggered: window.dispatchCommand("new-terminal") }
        MenuAction { keyHint: studyShortcuts.labels["new-workspace"] || ""; text: "New workspace…"; enabled: window.canCreateWorkspace; onTriggered: window.dispatchCommand("new-workspace") }
        MenuRule {}
        MenuAction { objectName: "renameWorkspaceMenuItem"; text: "Rename workspace…"; keyHint: studyShortcuts.labels["rename-workspace"] || ""; enabled: window.spaces.length > 0 && !window.overview && !studySession.busy; onTriggered: window.editWorkspace("rename") }
        MenuAction { objectName: "workspaceFolderMenuItem"; text: "Workspace folder…"; keyHint: studyShortcuts.labels["workspace-folder"] || ""; enabled: window.spaces.length > 0 && !window.overview && !studySession.busy; onTriggered: window.editWorkspace("folder") }
        MenuAction { objectName: "removeWorkspaceMenuItem"; text: "Remove workspace…"; enabled: window.spaces.length > 0 && !studySession.busy; onTriggered: window.requestWorkspaceClose(window.spaceIds[window.workspace]) }
        MenuRule {}
        MenuAction { objectName: "appearanceMenuItem"; text: "Appearance…"; keyHint: studyShortcuts.labels["appearance"] || ""; enabled: window.spaces.length > 0 && !window.overview; onTriggered: window.openAppearance() }
        MenuAction { objectName: "terminalSearchMenuItem"; text: window.overview ? "Find in Overview" : "Find in terminal"; keyHint: studyShortcuts.labels["terminal-search"] || ""; enabled: commandRegistry.find("terminal-search").available; onTriggered: window.dispatchCommand("terminal-search") }
        MenuAction { objectName: "fontSettingsMenuItem"; text: "Terminal text…"; keyHint: studyShortcuts.labels["font-settings"] || ""; onTriggered: window.openFontSettings() }
        MenuAction { objectName: "configurationMenuItem"; text: "Configuration…"; onTriggered: configurationDialog.open() }
        MenuAction { objectName: "shortcutSettingsMenuItem"; text: "Shortcut settings…"; keyHint: studyShortcuts.labels["shortcut-settings"] || ""; onTriggered: window.openShortcutSettings() }
        MenuAction { keyHint: studyShortcuts.labels["help"] || ""; text: "Keyboard shortcuts"; onTriggered: help.open() }
        MenuAction { text: "Reduced motion"; checkable: true; checked: window.reducedMotion; onTriggered: window.motionOverride = checked }
        MenuRule {}
        MenuAction { objectName: "forwardKeyMenuItem"; text: "Send next key to terminal"; keyHint: studyShortcuts.labels["forward"] || ""; enabled: commandRegistry.find("forward").available; onTriggered: { window.pendingForward = true; appMenu.close() } }
        MenuAction { text: "Copy selection"; enabled: window.selected >= 0; onTriggered: window.panel(window.selected).terminal.copy() }
        MenuAction { text: "Paste"; enabled: window.selected >= 0 && !window.overview; onTriggered: { window.panel(window.selected).terminal.paste(); window.panel(window.selected).terminal.forceActiveFocus() } }
        MenuRule {}
        MenuAction { text: studySession.savedSession ? "Refresh workspaces" : "Refresh study workspaces"; enabled: !studySession.busy; onTriggered: studySession.refresh() }
        MenuRule {}
        MenuAction { objectName: "sessionStopMenuItem"; text: "Stop all terminals…"; enabled: terminalModel.count > 0 && !studySession.busy; onTriggered: window.requestSessionStop() }
        MenuAction { text: studySession.savedSession ? "Close window" : "Close UI study"; onTriggered: window.close() }
        MenuRule {}
        MenuAction { objectName: "aboutMenuItem"; text: "About Band"; onTriggered: about.open() }
    }
    ChromeMenu {
        tones: window.chrome
        id: paneMenu
        onClosed: window.restoreAfterPopup()
        MenuAction { objectName: "terminalInfoMenuItem"; text: "Terminal details…"; enabled: window.identityIndex(window.paneMenuTarget) >= 0; onTriggered: window.openRuntimeInfo(window.paneMenuTarget) }
        MenuAction { objectName: "terminalColorsMenuItem"; text: "Terminal colors…"; enabled: window.identityIndex(window.paneMenuTarget) >= 0 && !window.overview; onTriggered: window.openTerminalColors(window.paneMenuTarget) }
        MenuAction { objectName: "agentInfoMenuItem"; text: "Agent details…"; enabled: window.identityIndex(window.paneMenuTarget) >= 0; onTriggered: window.openAgentInfo(window.paneMenuTarget) }
        MenuRule {}
        MenuAction { objectName: "terminalCloseMenuItem"; text: window.exitedTerminal(window.paneMenuTarget) ? "Close pane" : "Close terminal…"; keyHint: studyShortcuts.labels["close-terminal"] || ""; enabled: window.selected >= 0 && !studySession.busy; onTriggered: window.closePane(window.paneMenuTarget) }
        MenuRule {}
        MenuAction { text: "Cycle width"; keyHint: studyShortcuts.labels["cycle-width"] || ""; onTriggered: window.dispatchCommand("cycle-width") }
        MenuAction { text: "Toggle full width"; keyHint: studyShortcuts.labels["full-width"] || ""; onTriggered: window.dispatchCommand("full-width") }
        MenuRule {}
        MenuAction { objectName: "moveLeftMenuItem"; text: "Move terminal left"; keyHint: studyShortcuts.labels["move-left"] || ""; enabled: window.canReorder(-1); onTriggered: window.reorderSelected(-1) }
        MenuAction { objectName: "moveRightMenuItem"; text: "Move terminal right"; keyHint: studyShortcuts.labels["move-right"] || ""; enabled: window.canReorder(1); onTriggered: window.reorderSelected(1) }
        MenuRule {}
        MenuAction { text: "Move to workspace above"; enabled: window.canMoveWorkspace(-1); onTriggered: window.moveSelectedWorkspace(-1) }
        MenuAction { text: "Move to workspace below"; enabled: window.canMoveWorkspace(1); onTriggered: window.moveSelectedWorkspace(1) }
    }
    Rectangle {
        visible: window.moveNotice.length > 0 && studySession.error.length === 0 && window.moveKeyContext
        anchors.bottom: parent.bottom; anchors.bottomMargin: Design.spaceXl; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 100, moveNoticeText.implicitWidth + 32); height: 42; radius: Design.radiusL; z: 4
        color: window.chrome.surface; border.color: window.chrome.border
        Label { id: moveNoticeText; anchors.centerIn: parent; text: window.moveNotice; color: window.chrome.foreground; font.pixelSize: Design.textBody }
    }
    Timer { id: moveNoticeTimer; interval: 3000; onTriggered: window.moveNotice = "" }
    Rectangle {
        visible: (studySession.error.length > 0 || studySession.catalogNotice.length > 0) && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        // A notice in the window's tones, marked by the waiting color.
        width: Math.min(window.width - 100, 680); height: 76; color: window.chrome.raised; radius: Design.radiusL; z: 4
        border.width: Design.line; border.color: window.chrome.waiting
        Label { x: 16; y: 12; width: parent.width - 120; text: studySession.error || studySession.catalogNotice; wrapMode: Text.Wrap; color: window.chrome.foreground }
        QuietButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Refresh"; enabled: !studySession.busy; onClicked: studySession.refresh() }
    }
    ThemePicker {
        id: appearance; anchors.centerIn: parent
        tones: window.chrome
        readonly property bool terminalMode: window.appearanceTerminal !== ""
        readonly property int terminalIndex: window.identityIndex(window.appearanceTerminal)
        readonly property int targetSpace: window.spaceIds.indexOf(window.appearanceWorkspace)
        saveError: terminalMode ? studyTerminalAppearance.error : studyWorkspaceAppearance.error
        heading: terminalMode ? "Terminal colors" : "Appearance"
        scope: terminalMode ? "this terminal only" : "all terminals in this workspace"
        useDefaultText: terminalMode ? "Use workspace" : "Use default"
        usingDefaultText: terminalMode ? "Using workspace" : "Using default"
        note: window.appearanceNote
        retainedTheme: studyThemes.theme(window.previewTheme)
        directoryPath: studyThemes.themesPath; directoryStatus: studyThemes.directoryStatus
        onFolderRequested: studyThemes.openThemesDirectory()
        catalog: studyThemes.catalog; themeIndex: window.previewTheme
        workspaceName: terminalMode ? (terminalIndex >= 0 && window.panel(terminalIndex) ? window.panel(terminalIndex).facts.title : "") : window.spaces[targetSpace] || ""
        onPreviewSelected: index => { window.previewTheme = index; window.previewDefault = false }
        followsDefault: window.previewDefault
        defaultName: studyThemes.theme(terminalMode ? (targetSpace >= 0 ? window.effectiveTheme(targetSpace) : 0) : studyConfiguration.themeIndex).name
        onDefaultRequested: { window.previewTheme = terminalMode && targetSpace >= 0 ? window.effectiveTheme(targetSpace) : studyConfiguration.themeIndex; window.previewDefault = true }
        // Saves the selection as Band's default; this workspace then follows it.
        canMakeDefault: !terminalMode
        onMakeDefaultRequested: { if (studyAppearance.saveDefault(window.previewTheme)) { window.previewDefault = true; applySelected() } }
        onApplySelected: {
            const theme = window.previewDefault ? ({}) : studyThemes.snapshot(window.previewTheme)
            if (terminalMode) {
                if (terminalIndex < 0 || !studyTerminalAppearance.apply(window.appearanceTerminal, window.previewDefault, theme)) return
            } else if (targetSpace < 0 || !studyWorkspaceAppearance.apply(window.appearanceWorkspace, window.previewDefault, theme)) return
            window.applySharedAppearance(); close()
        }
        onClosed: {
            if (terminalMode) studyTerminalAppearance.end(); else studyWorkspaceAppearance.end()
            window.appearanceTerminal = ""
            if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
        }
    }
    FontSettings {
        id: fontSettings; anchors.centerIn: parent; tones: window.chrome; preferences: studyFonts
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    Rectangle {
        visible: window.forwardingArmed
        x: 60; y: 44; width: Math.min(520, window.width - 76); height: 44; radius: Design.radiusM; z: 7
        color: window.chrome.raised; border.color: window.chrome.accent
        Label { anchors.fill: parent; anchors.margins: Design.spaceL; anchors.rightMargin: 85; text: "Next key goes to this terminal · Escape cancels"; color: window.chrome.foreground; elide: Text.ElideRight }
        QuietButton { objectName: "cancelForwardKey"; anchors.right: parent.right; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; text: "Cancel"; onClicked: if (window.selected >= 0) window.panel(window.selected).terminal.cancelForwarding() }
    }
    Connections { target: studyView; function onChanged() { window.fontWarningDismissed = false } }
    property bool fontWarningDismissed: false
    Rectangle {
        visible: (studyFonts.error.length > 0 || studyShortcuts.error.length > 0 || studyView.error.length > 0 || studyConfiguration.error.length > 0 || studyWorkspaceAppearance.error.length > 0) && !window.fontWarningDismissed; x: 60; y: 44; width: Math.min(600, window.width - 76); height: 56; radius: Design.radiusM; z: 6; color: window.chrome.raised
        QuietButton { anchors.right: parent.right; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; text: "Dismiss"; onClicked: window.fontWarningDismissed = true }
        Label { anchors.fill: parent; anchors.margins: Design.spaceL; anchors.rightMargin: 94; color: window.chrome.waiting; wrapMode: Text.Wrap; textFormat: Text.PlainText; text: [studyWorkspaceAppearance.error, studyConfiguration.error, studyFonts.error, studyShortcuts.error, studyView.error].filter(value => value.length > 0).join("\n") }
    }
    AppCatalog {
        id: appCatalog; objectName: "appCatalog"; anchors.centerIn: parent; host: window
        onChosen: (id, install, via) => { window.pendingApp = {id: id, install: install, via: via}; close() }
        onHomebrewRequested: { window.pendingApp = {id: "", install: "", via: "homebrew"}; close() }
        onSetupRequested: id => { window.pendingApp = {id: id, install: "", via: "setup"}; close() }
        onClosed: {
            const choice = window.pendingApp; window.pendingApp = null
            Qt.callLater(function() {
                if (choice && choice.via === "homebrew") window.installHomebrew()
                else if (choice && choice.via === "setup") window.runSetup(choice.id)
                else if (choice) window.chooseApp(choice.id, choice.install)
                else window.restoreAfterPopup()
            })
        }
    }
    CommandPalette {
        id: commands; objectName: "commandPalette"; x: Math.round((parent.width - width) / 2); y: Math.round(Math.max(32, parent.height * 0.14))
        tones: window.chrome; actions: commandRegistry.entries
        contextLabel: (window.spaces[window.workspace] || "No workspace") + (window.selected >= 0 && window.panel(window.selected) ? " · " + window.panel(window.selected).facts.title : " · Empty workspace")
        onChosen: id => { window.pendingCommand = id; close() }
        onClosed: {
            const action = window.pendingCommand; window.pendingCommand = ""
            // Reject stale context rather than applying an action to a different pane.
            const targetWorkspace = window.commandWorkspace, targetTerminal = window.commandTerminal
            Qt.callLater(function() {
                const sameContext = targetWorkspace === (window.spaceIds[window.workspace] || "") && targetTerminal === (window.selected >= 0 ? terminalModel.get(window.selected).terminalId : "")
                if (action && sameContext) window.dispatchCommand(action)
                window.restoreAfterPopup()
            })
        }
    }
    Dialog {
        id: terminalAction; objectName: "terminalActionDialog"; anchors.centerIn: parent; modal: true; focus: true
        width: Math.min(440, window.width - 64); padding: Design.space2xl
        closePolicy: studySession.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
        title: window.actionKind === "session-stop" ? "Stop all terminals?" : window.actionKind === "workspace-close" ? "Remove workspace?" : window.actionKind === "close" ? "Close terminal?" : "Move terminal here?"
        background: Rectangle { color: window.chrome.raised; radius: Design.radiusXl; border.color: window.chrome.border }
        header: Label { text: terminalAction.title; padding: Design.space2xl; bottomPadding: Design.spaceM; color: window.chrome.foreground; font.pixelSize: Design.textTitle }
        contentItem: Column {
            spacing: Design.spaceXl
            Label { width: parent.width; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: window.chrome.muted; font.pixelSize: Design.textSmall; text: window.actionKind === "session-stop" ? window.stopTargets.length + (window.stopTargets.length === 1 ? " saved terminal" : " saved terminals") + " · All workspaces and windows in this session" : window.actionKind === "workspace-close" ? window.workspaceRemovalContext : window.actionContext }
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: window.chrome.foreground; text: window.actionKind === "session-stop" ? "Stop these terminals’ shells and foreground jobs. Keep workspaces, folders, names and saved appearance. Terminals created after this confirmation opened are excluded. Windows stay open; starting fresh shells later requires an explicit action." : window.actionKind === "workspace-close" ? "Remove this workspace and all of its terminals. Running processes in this workspace will be stopped. Other workspaces keep running." : window.actionKind === "close" ? "Stop this terminal’s process and remove it from the workspace. Other terminals keep running." : "Continue the same process in this window. The other window will release control." }
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: window.chrome.waiting; visible: window.actionKind === "session-stop" && !window.stopTargetsValid; text: "A confirmed terminal was removed. Cancel and reopen this confirmation to review the current session." }
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: window.chrome.waiting; text: studySession.error; visible: text.length > 0 }
            Row {
                x: parent.width - width; spacing: Design.spaceXs   // the decision sits on the right
                QuietButton { id: cancelTerminalAction; objectName: "cancelTerminalAction"; Keys.onPressed: event => { if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { terminalAction.close(); event.accepted = true } else event.accepted = false }; text: "Cancel"; enabled: !studySession.busy; onClicked: terminalAction.close() }
                QuietButton { id: confirmTerminalAction; objectName: "confirmTerminalAction"; primary: true; Keys.onPressed: event => { if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { window.performTerminalAction(); event.accepted = true } else event.accepted = false }; text: studySession.busy ? "Working…" : window.actionKind === "session-stop" ? "Stop terminals" : window.actionKind === "workspace-close" ? "Remove workspace" : window.actionKind === "close" ? "Close terminal" : "Move here"; enabled: !studySession.busy && (window.actionKind !== "session-stop" || window.stopTargetsValid); onClicked: window.performTerminalAction() }
            }
        }
        onOpened: { studySession.clearError(); cancelTerminalAction.forceActiveFocus(Qt.TabFocusReason) }
        onClosed: { window.stopTargets = []; Qt.callLater(window.restoreAfterPopup) }
    }
    component ProjectField: TextField {
        selectByMouse: true; color: window.chrome.foreground; selectionColor: window.accent; selectedTextColor: window.chrome.background
        placeholderTextColor: window.chrome.muted; padding: Design.spaceL
        background: Rectangle { radius: Design.radiusM; color: window.chrome.background; border.color: parent.activeFocus ? window.accent : window.chrome.border }
    }
    // Add host: a name, an ssh destination (with ~/.ssh/config aliases to pick)
    // and an optional remote folder, saved in Band's own host list.
    Dialog {
        id: hostDialog; objectName: "hostDialog"
        property var suggestions: []
        property string theme: ""
        implicitWidth: 460; width: Math.min(460, window.width - 80); anchors.centerIn: parent; modal: true; padding: Design.space2xl
        title: "Add host"
        background: Rectangle { color: window.chrome.raised; radius: Design.radiusXl; border.color: window.chrome.border }
        header: Label { text: hostDialog.title; padding: Design.space2xl; bottomPadding: Design.spaceS; color: window.chrome.foreground; font.pixelSize: Design.textTitle }
        onOpened: hostDestination.forceActiveFocus()
        function save() {
            const id = studyHosts.freeId(hostName.text.trim() || hostDestination.text.trim(), window.hostDefinitions)
            if (!studyHosts.save({id: id, name: hostName.text.trim(), destination: hostDestination.text.trim(), directory: hostFolder.text.trim(), theme: hostDialog.theme})) return
            hostDialog.close(); window.connectHost(id)
        }
        contentItem: Column {
            spacing: Design.spaceL
            Label { text: "Destination"; color: window.chrome.foreground }
            ProjectField { id: hostDestination; objectName: "hostDestinationInput"; width: parent.width; placeholderText: "An ssh alias, host or user@host"; onAccepted: hostName.forceActiveFocus() }
            Flow {
                width: parent.width; spacing: Design.spaceS; visible: hostDialog.suggestions.length > 0
                Label { height: 32; verticalAlignment: Text.AlignVCenter; text: "From ~/.ssh/config"; color: window.chrome.muted; font.pixelSize: Design.textSmall; rightPadding: Design.spaceXs }
                Repeater {
                    model: hostDialog.suggestions.slice(0, 12)
                    QuietButton {
                        required property string modelData
                        required property int index
                        objectName: "hostSuggestion" + index; focusPolicy: Qt.NoFocus; text: modelData
                        selected: hostDestination.text === modelData
                        onClicked: hostDestination.text = modelData
                    }
                }
            }
            Label { text: "Name"; color: window.chrome.foreground }
            ProjectField { id: hostName; objectName: "hostNameInput"; width: parent.width; placeholderText: hostDestination.text || "Shown in the sidebar"; maximumLength: 63; onAccepted: hostFolder.forceActiveFocus() }
            Label { text: "Remote folder (optional)"; color: window.chrome.foreground }
            ProjectField { id: hostFolder; objectName: "hostFolderInput"; width: parent.width; placeholderText: "~/src/project"; onAccepted: hostDialog.save() }
            // Colors for every pane on this host; a terminal's own colors still win.
            Label { text: "Colors"; color: window.chrome.foreground }
            Flow {
                width: parent.width; spacing: Design.spaceS
                QuietButton { objectName: "hostThemeWorkspace"; focusPolicy: Qt.NoFocus; text: "Workspace colors"; selected: hostDialog.theme === ""; onClicked: hostDialog.theme = "" }
                Repeater {
                    model: studyThemes.catalog.filter(theme => !theme.imported).slice(0, 12)
                    QuietButton {
                        required property var modelData
                        required property int index
                        objectName: "hostTheme" + index; focusPolicy: Qt.NoFocus; text: modelData.name
                        selected: hostDialog.theme === modelData.name; onClicked: hostDialog.theme = modelData.name
                    }
                }
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textSmall; color: studyHosts.error ? window.chrome.waiting : window.chrome.muted
                text: studyHosts.error || "Saved by Band beside its other settings, then opened in this workspace. Keys, passwords and ssh options stay with ssh."
            }
            Row {
                x: parent.width - width; spacing: Design.spaceXs   // the decision sits on the right
                QuietButton { text: "Cancel"; onClicked: hostDialog.close() }
                QuietButton { objectName: "saveHostButton"; primary: true; text: "Connect"; enabled: hostDestination.text.trim().length > 0; onClicked: hostDialog.save() }
            }
        }
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    // New host pane: the saved hosts, with reachability; Enter or a click
    // connects in this workspace. Add host… defines a new destination.
    Dialog {
        id: hostChooser; objectName: "hostChooser"
        implicitWidth: 460; width: Math.min(460, window.width - 80); anchors.centerIn: parent; modal: true; padding: Design.spaceL
        title: "New host pane"
        background: Rectangle { color: window.chrome.raised; radius: Design.radiusXl; border.color: window.chrome.border }
        header: Label { text: hostChooser.title; padding: Design.space2xl; bottomPadding: Design.spaceXs; color: window.chrome.foreground; font.pixelSize: Design.textTitle }
        function choose(id) { close(); window.connectHost(id) }
        // Runs here, not in the row: removing rebuilds the rows.
        function remove(id) { studyHosts.remove(id); if (!window.hostDefinitions.length) close() }
        // The list keeps keyboard focus and marks the current host, so rows
        // rebuilt after a removal cannot take focus with them.
        onOpened: { studyHosts.check(window.hostDefinitions); hostChoices.currentIndex = 0; hostChoices.forceActiveFocus(Qt.TabFocusReason) }
        onClosed: Qt.callLater(window.restoreAfterPopup)
        // Reachability while the chooser is open: on opening, then every minute.
        Timer { interval: 60000; repeat: true; running: hostChooser.visible; onTriggered: studyHosts.check(window.hostDefinitions) }
        contentItem: Column {
            spacing: Design.space2xs
            Label {
                width: parent.width; visible: !window.canCreateTerminal; padding: Design.spaceM; wrapMode: Text.Wrap
                text: "Already at " + window.maxTerminals + " terminals; close one to connect."; color: window.chrome.waiting; font.pixelSize: Design.textSmall
            }
            ListView {
                id: hostChoices; objectName: "hostChoices"
                width: parent.width; height: Math.min(contentHeight, window.height * 0.5); clip: true; spacing: Design.space2xs
                boundsBehavior: Flickable.StopAtBounds; keyNavigationWraps: false; highlightMoveDuration: 0
                model: hostChooser.visible ? window.hostDefinitions : []
                onCountChanged: if (count && currentIndex >= count) currentIndex = count - 1
                Keys.onPressed: event => {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && currentIndex >= 0) { hostChooser.choose(model[currentIndex].id); event.accepted = true }
                    else event.accepted = false
                }
                delegate: Item {
                    id: hostRow
                    required property int index
                    required property var modelData
                    readonly property var panes: window.hostPanes(modelData.id)
                    readonly property string reach: studyHosts.reachability[modelData.id] || ""
                    readonly property bool current: ListView.isCurrentItem
                    width: hostChoices.width; height: 44
                    HoverHandler { id: hostHover }
                    QuietButton {
                        id: hostEntry; objectName: "hostChoice" + hostRow.index
                        width: parent.width; height: 44; padding: Design.spaceM; focusPolicy: Qt.NoFocus
                        selected: hostRow.current && hostChoices.activeFocus
                        hint: hostRow.modelData.name + " · " + hostRow.modelData.destination + (hostRow.panes.length ? " · " + hostRow.panes.length + " open" : "")
                            + (hostRow.reach === "reachable" ? " · reachable" : hostRow.reach === "unreachable" ? " · not reachable" : hostRow.reach === "unknown" ? " · reachability unknown" : "")
                        contentItem: Item {
                            ChromeIcon { id: hostIcon; x: 0; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; name: "host"; color: window.chrome.muted }
                            // Reachable: a ready dot; not reachable: a waiting dot; unknown: none.
                            Rectangle {
                                objectName: "hostReach" + hostRow.index; x: hostIcon.x + 12; y: hostIcon.y + 11; width: 8; height: 8; radius: 4
                                visible: hostRow.reach === "reachable" || hostRow.reach === "unreachable"; border.width: 1.5; border.color: window.chrome.raised
                                color: hostRow.reach === "reachable" ? window.chrome.ready : window.chrome.waiting
                            }
                            Column {
                                anchors.left: hostIcon.right; anchors.leftMargin: Design.spaceL; anchors.right: parent.right; anchors.rightMargin: hostRow.modelData.saved ? 34 : 0; anchors.verticalCenter: parent.verticalCenter; spacing: 1
                                Text { width: parent.width; text: hostRow.modelData.name + (hostRow.panes.length ? "  ·  " + hostRow.panes.length + " open" : ""); textFormat: Text.PlainText; elide: Text.ElideRight; color: window.chrome.foreground; font.pixelSize: Design.textBody; font.weight: Font.DemiBold }
                                Text { width: parent.width; text: hostRow.modelData.destination + (hostRow.modelData.directory ? " · " + hostRow.modelData.directory : ""); textFormat: Text.PlainText; elide: Text.ElideMiddle; color: window.chrome.muted; font.pixelSize: Design.textCaption }
                            }
                        }
                        onClicked: hostChooser.choose(hostRow.modelData.id)
                    }
                    // Hosts Band saved can be removed here; config.conf hosts stay as written.
                    QuietButton {
                        objectName: "removeHost" + hostRow.index; z: 2; visible: !!hostRow.modelData.saved
                        opacity: hostHover.hovered || (hostRow.current && hostChoices.activeFocus) ? 1 : 0
                        anchors.right: hostEntry.right; anchors.rightMargin: Design.spaceXs; anchors.verticalCenter: hostEntry.verticalCenter
                        iconName: "close"; height: 28; focusPolicy: Qt.NoFocus
                        hint: "Remove " + hostRow.modelData.name + " from Band's hosts"
                        onClicked: hostChooser.remove(hostRow.modelData.id)
                    }
                }
            }
        }
        footer: Item {
            implicitHeight: 56
            QuietButton {
                objectName: "addHostButton"; x: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; text: "Add host…"; focusPolicy: Qt.NoFocus
                onClicked: { hostChooser.close(); Qt.callLater(window.addHost) }
            }
            QuietButton { anchors.right: parent.right; anchors.rightMargin: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; text: "Cancel"; focusPolicy: Qt.NoFocus; onClicked: hostChooser.close() }
        }
    }
    Dialog {
        id: createDialog; objectName: "createWorkspaceDialog"
        onOpened: {
            const field = window.workspaceFormMode === "folder" ? newFolder : newName
            field.forceActiveFocus(); field.deselect(); field.cursorPosition = field.text.length
        }
        implicitWidth: 460
        title: window.workspaceFormMode === "rename" ? "Rename workspace" : window.workspaceFormMode === "folder" ? "Workspace folder" : "New workspace"; anchors.centerIn: parent; modal: true; width: Math.min(460, window.width - 80)
        padding: Design.space2xl
        background: Rectangle { color: window.chrome.raised; radius: Design.radiusXl; border.color: window.chrome.border }
        header: Label { text: createDialog.title; padding: Design.space2xl; bottomPadding: Design.spaceS; color: window.chrome.foreground; font.pixelSize: Design.textTitle }
        closePolicy: studySession.busy || window.folderBrowserOpen ? Popup.NoAutoClose : Popup.CloseOnEscape
        contentItem: Column {
            spacing: Design.spaceL
            Label { text: "Name"; visible: window.workspaceFormMode !== "folder"; color: window.chrome.foreground }
            ProjectField {
                id: newName; objectName: "workspaceNameInput"; width: parent.width; placeholderText: "Project name"
                visible: window.workspaceFormMode !== "folder"; enabled: !studySession.busy; maximumLength: 63
                onAccepted: { if (window.workspaceFormMode === "rename") { if (submitWorkspace.enabled) submitWorkspace.clicked() } else newFolder.forceActiveFocus() }
            }
            Label { text: "Folder"; visible: window.workspaceFormMode !== "rename"; color: window.chrome.foreground }
            Row {
                width: parent.width; spacing: Design.spaceM; visible: window.workspaceFormMode !== "rename"
                ProjectField {
                    id: newFolder; objectName: "workspaceFolderInput"; width: parent.width - browseFolder.width - parent.spacing; enabled: !studySession.busy && !studyWorktrees.busy
                    onAccepted: if (submitWorkspace.enabled) submitWorkspace.clicked()
                    onTextChanged: if (createDialog.visible && window.workspaceFormMode === "create") worktreeLookup.restart()
                    Timer { id: worktreeLookup; interval: 300; onTriggered: studyWorktrees.inspect(newFolder.text, studySession.defaultDirectory) }
                }
                QuietButton { id: browseFolder; objectName: "browseWorkspaceFolderButton"; text: "Browse…"; hint: "Choose a folder"; enabled: !studySession.busy; onClicked: window.browseWorkspaceFolder() }
            }
            Label { width: parent.width; visible: window.workspaceFormMode !== "rename"; text: "Used by new terminals. Running terminals keep their current folders."; wrapMode: Text.Wrap; color: window.chrome.muted; font.pixelSize: Design.textSmall }
            // In a Git repository: open a worktree, or add one on a new branch.
            Column {
                width: parent.width; spacing: Design.spaceM; visible: window.workspaceFormMode === "create" && studyWorktrees.repository.length > 0
                Label { text: "Git worktree"; color: window.chrome.foreground }
                Flow {
                    width: parent.width; spacing: Design.spaceS
                    Repeater {
                        model: studyWorktrees.worktrees
                        QuietButton {
                            required property var modelData
                            required property int index
                            objectName: "worktreeChoice" + index; focusPolicy: Qt.NoFocus
                            text: modelData.branch || modelData.path.split("/").pop(); hint: modelData.path
                            selected: newFolder.text === modelData.path
                            onClicked: { newFolder.text = modelData.path; newBranch.text = ""; if (!newName.text.trim()) newName.text = text }
                        }
                    }
                }
                ProjectField {
                    id: newBranch; objectName: "workspaceBranchInput"; width: parent.width; enabled: !studySession.busy && !studyWorktrees.busy
                    placeholderText: "Or a branch for a new worktree"
                    onAccepted: if (submitWorkspace.enabled) submitWorkspace.clicked()
                }
                Label {
                    width: parent.width; wrapMode: Text.Wrap; color: studyWorktrees.error ? window.chrome.waiting : window.chrome.muted; font.pixelSize: Design.textSmall
                    text: studyWorktrees.error || (newBranch.text.trim() ? "Adds a worktree at " + (studyWorktrees.pathFor(newBranch.text.trim()) || "…") + " (git worktree add)." : "")
                    visible: text.length > 0
                }
            }
            Label { width: parent.width; text: studySession.error; visible: text.length > 0; wrapMode: Text.Wrap; color: window.chrome.waiting }
            Row {
                x: parent.width - width; spacing: Design.spaceXs   // the decision sits on the right
                QuietButton { text: "Cancel"; enabled: !studySession.busy; onClicked: createDialog.close() }
                QuietButton {
                    id: submitWorkspace; objectName: "submitWorkspaceButton"; primary: true
                    readonly property bool withBranch: newBranch.visible && newBranch.text.trim().length > 0
                    text: studyWorktrees.busy ? "Adding worktree…" : studySession.busy ? (window.workspaceFormMode === "create" ? "Creating…" : "Saving…") : window.workspaceFormMode === "create" ? "Create" : "Save"
                    enabled: !studySession.busy && !studyWorktrees.busy && (window.workspaceFormMode === "folder" || newName.text.trim().length > 0 || withBranch) && (window.workspaceFormMode === "rename" || newFolder.text.trim().length > 0)
                    onClicked: window.submitWorkspaceForm()
                }
            }
        }
        Component {
            id: folderBrowserFactory
            NativeDialogs.FolderDialog {
                id: browser; objectName: "workspaceFolderBrowser"
                parentWindow: window; title: "Choose workspace folder"; acceptLabel: "Choose folder"
                modality: Qt.WindowModal
                // Automated native-input checks use Qt's fallback, independent of host portals.
                options: testFolderDialogs ? NativeDialogs.FolderDialog.DontUseNativeDialog : 0
                Component.onCompleted: if (testFolderDialogs && "popupType" in browser) browser.popupType = Popup.Item
                onAccepted: {
                    const path = studySession.folderPath(selectedFolder)
                    if (createDialog.visible && path.length > 0) { newFolder.text = path; newFolder.deselect(); newFolder.cursorPosition = newFolder.text.length }
                    Qt.callLater(window.restoreFolderFocus)
                }
                onRejected: Qt.callLater(window.restoreFolderFocus)
            }
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    ShortcutSettings {
        id: shortcutSettings; anchors.centerIn: parent
        tones: window.chrome; preferences: studyShortcuts; actions: commandRegistry.entries
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Binding { target: studyConfiguration; property: "editing"; value: shortcutSettings.visible || fontSettings.visible || appearance.visible || createDialog.visible }
    readonly property bool configurationOpen: configurationDialog.visible
    TerminalInformation {
        id: terminalInformation; objectName: "terminalInformation"; anchors.centerIn: parent
        tones: window.chrome; inspector: studyRuntimeInfo; pane: window.runtimeInfoPane
        contextLabel: window.runtimeInfoPane ? window.spaces[window.runtimeInfoPane.spaceIndex] + " · " + window.runtimeInfoPane.facts.title : "Terminal unavailable"
        onRefreshRequested: window.refreshRuntimeInfo()
        onClosed: { studyRuntimeInfo.clear(); Qt.callLater(window.restoreAfterPopup) }
    }
    AgentInformation {
        id: agentInformation; objectName: "agentInformation"; anchors.centerIn: parent; tones: window.chrome; model: studyAgentInfo
        facts: window.agentInfoPane ? window.agentInfoPane.facts : ({})
        onTerminalDetailsRequested: window.openRuntimeInfo(window.agentInfoTarget)
        contextLabel: window.agentInfoPane ? window.spaces[window.agentInfoPane.spaceIndex] + " · " + window.agentInfoPane.facts.title : "Terminal unavailable"
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    Configuration {
        id: configurationDialog; anchors.centerIn: parent; tones: window.chrome; settings: studyConfiguration
        navigationInherited: window.navigationOverride === null; motionInherited: window.motionOverride === null
        navigationPinned: window.navigationPinned; reducedMotion: window.reducedMotion
        onViewDefaultsRequested: { window.navigationOverride = null; window.motionOverride = null }
        workspaceAppearancePath: studyWorkspaceAppearance.directory
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    Connections { target: studyConfiguration; function onReloadFailed() { window.fontWarningDismissed = false } }
    readonly property bool aboutOpen: about.visible
    readonly property bool aboutDetailsReady: about.detailsReady
    About {
        id: about; objectName: "aboutDialog"; anchors.centerIn: parent
        tones: window.chrome; build: studyBuild; titleBar: about.visible ? window.titleBarReport() : ""
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    Dialog {
        id: help; implicitWidth: 560; implicitHeight: 460; width: Math.min(implicitWidth, window.width - 64); height: Math.min(implicitHeight, window.height - 64); title: "Keyboard shortcuts"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
        padding: Design.spaceXl
        background: Rectangle { color: window.chrome.raised; radius: Design.radiusXl; border.color: window.chrome.border }
        header: Label { text: help.title; padding: Design.space2xl; bottomPadding: Design.spaceM; color: window.chrome.foreground; font.pixelSize: Design.textTitle }
        footer: Item {
            implicitHeight: 56
            QuietButton { x: 16; anchors.verticalCenter: parent.verticalCenter; text: "Edit shortcuts…"; onClicked: window.openShortcutSettings() }
            QuietButton { anchors.right: parent.right; anchors.rightMargin: Design.spaceXl; anchors.verticalCenter: parent.verticalCenter; text: "Close"; onClicked: help.close() }
        }
        contentItem: ListView {
            clip: true; spacing: Design.spaceXs
            model: commandRegistry.entries.filter(entry => entry.key.length > 0)
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                required property var modelData
                width: ListView.view.width; height: 34
                Label { x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.56; elide: Text.ElideRight; text: modelData.title; textFormat: Text.PlainText; color: window.chrome.foreground }
                Accessible.role: Accessible.StaticText; Accessible.name: modelData.title + ", " + modelData.key
                KeyChips { anchors.right: parent.right; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: parent.verticalCenter; keys: modelData.key; tones: window.chrome }
            }
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    readonly property bool globalKeyContext: !overviewSearch.activeFocus && !navigation.searchFocused && dragSlot < 0 && !terminalSearch.visible && !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen && !terminalInformation.visible && !agentInformation.visible && !about.visible && !configurationDialog.visible && !help.visible && !drawer.visible && !appMenu.visible && !paneMenu.visible && !hostChooser.visible && !hostDialog.visible
    function activateShortcut(id) {
        // Boundary navigation still reveals/focuses the selected pane after scrolling.
        if (id === "previous-terminal") navigate(-1)
        else if (id === "next-terminal") navigate(1)
        else if (id === "previous-workspace") chooseWorkspace(workspace - 1)
        else if (id === "next-workspace") chooseWorkspace(workspace + 1)
        else if (id === "move-up") moveSelectedWorkspace(-1)
        else if (id === "move-down") moveSelectedWorkspace(1)
        else dispatchCommand(id)
    }
    Repeater {
        model: Object.keys(studyShortcuts.sequences).filter(id => studyShortcuts.sequences[id].length > 0)
        delegate: Item {
            id: bindingEntry; required property string modelData
            width: 0; height: 0
            Shortcut {
                sequence: studyShortcuts.sequences[bindingEntry.modelData]
                enabled: window.globalKeyContext
                onActivated: window.activateShortcut(bindingEntry.modelData)
            }
        }
    }
    Shortcut { sequence: "Escape"; enabled: window.overview && !overviewSearch.activeFocus && !navigation.searchFocused && !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen && !terminalInformation.visible && !agentInformation.visible && !about.visible && !configurationDialog.visible && !help.visible && !drawer.visible && !appMenu.visible; onActivated: { if (window.dragSlot >= 0) window.cancelDrag(); else window.toggleOverview() } }
    onActiveChanged: { if (!active) cancelDrag(); else if (appleTitle) syncTitleText() }
    // A frameless window on Linux resizes from its edges and corners.
    Repeater {
        model: window.ownTitle && !window.maximized ? [
            {edges: Qt.LeftEdge, x: 0, y: 6, w: 5, h: -12, cursor: Qt.SizeHorCursor}, {edges: Qt.RightEdge, x: -5, y: 6, w: 5, h: -12, cursor: Qt.SizeHorCursor},
            {edges: Qt.TopEdge, x: 6, y: 0, w: -12, h: 4, cursor: Qt.SizeVerCursor}, {edges: Qt.BottomEdge, x: 6, y: -5, w: -12, h: 5, cursor: Qt.SizeVerCursor},
            {edges: Qt.TopEdge | Qt.LeftEdge, x: 0, y: 0, w: 6, h: 6, cursor: Qt.SizeFDiagCursor}, {edges: Qt.BottomEdge | Qt.RightEdge, x: -6, y: -6, w: 6, h: 6, cursor: Qt.SizeFDiagCursor},
            {edges: Qt.TopEdge | Qt.RightEdge, x: -6, y: 0, w: 6, h: 6, cursor: Qt.SizeBDiagCursor}, {edges: Qt.BottomEdge | Qt.LeftEdge, x: 0, y: -6, w: 6, h: 6, cursor: Qt.SizeBDiagCursor}
        ] : []
        MouseArea {
            required property var modelData
            z: 100; x: modelData.x < 0 ? window.width + modelData.x : modelData.x; y: modelData.y < 0 ? window.height + modelData.y : modelData.y
            width: modelData.w <= 0 ? window.width + modelData.w : modelData.w; height: modelData.h <= 0 ? window.height + modelData.h : modelData.h
            cursorShape: modelData.cursor; onPressed: window.startSystemResize(modelData.edges)
        }
    }
    Component.onCompleted: { Qt.callLater(syncTitleText); studyApps.setDefinitions(window.appDefinitions); window.applyCatalog(studySession.initialCatalog, "initialize"); selected = remembered[0] ?? -1; Qt.callLater(function() { window.restoreView(studyView.initial); window.applySharedAppearance(); window.viewReady = true; window.scheduleViewSave(); if (studyWelcome.due) window.openWelcome(); else if (!window.overview && selected >= 0) window.panel(selected).terminal.forceActiveFocus() }) }
}
