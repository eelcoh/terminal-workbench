import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Dialogs as NativeDialogs
import Workbench.Probe
ApplicationWindow {
    id: window
    width: 1240; height: 760; minimumWidth: 680; minimumHeight: 440
    visible: true; title: studySession.savedSession ? "Terminal Workbench" : "Terminal Workbench · Qt UI study"; color: chrome.background
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
            workspaces[spaceIds[w]] = {focus: validFocus ? terminalModel.get(focus).terminalId : "", x: Math.max(0,x), theme: studyThemes.snapshot(workspaceThemes[w] || 0)}
        }
        for (let i = 0; i < terminalModel.count; ++i) terminals[terminalModel.get(i).terminalId] = {width: widths[i] || 0.5, full: fullWidths[i] || false}
        return {version: 1, window: {width: Math.max(680,Math.min(16384,Math.round(width))), height: Math.max(440,Math.min(16384,Math.round(height)))},
            workspace: spaceIds[workspace] || "", terminal: selected >= 0 && selected < terminalModel.count ? terminalModel.get(selected).terminalId : "",
            overview: overview, agents: agentsOnly, needs: needsOnly, motion: reducedMotion, overview_y: overview ? Math.max(0,deck.contentY) : 0,
            workspaces: workspaces, terminals: terminals}
    }
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
            const theme = value ? studyThemes.restoreSnapshot(value.theme) : 0
            themes.push(Math.max(0,theme))
            if (theme < 0) studyView.invalidate("A saved workspace theme is invalid. Using default colors; view file left untouched.")
        }
        workspaceThemes = themes; remembered = focus; positions = scroll
        workspace = Math.max(0,spaceIds.indexOf(saved.workspace)); selected = identityIndex(saved.terminal)
        if (selected >= 0) workspace = terminalModel.get(selected).spaceIndex
        else selected = remembered[workspace] ?? -1
        reducedMotion = saved.motion; agentsOnly = saved.agents; needsOnly = saved.needs; overview = saved.overview
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
    onSelectedChanged: scheduleViewSave()
    onWidthsChanged: scheduleViewSave()
    onFullWidthsChanged: scheduleViewSave()
    onRememberedChanged: scheduleViewSave()
    onPositionsChanged: scheduleViewSave()
    onWorkspaceThemesChanged: scheduleViewSave()
    onSpaceIdsChanged: scheduleViewSave()
    onAgentsOnlyChanged: scheduleViewSave()
    onNeedsOnlyChanged: scheduleViewSave()
    onReducedMotionChanged: scheduleViewSave()
    function requestTerminalAction(identity, kind) {
        if (studySession.busy || terminalAction.visible) return
        actionTarget = identity; actionKind = kind; const i = identityIndex(identity); actionContext = i < 0 ? "" : panel(i).facts.title + " · " + spaces[terminalModel.get(i).spaceIndex]; paneMenu.close()
        Qt.callLater(function() { if (identityIndex(actionTarget) >= 0) terminalAction.open() })
    }
    function performTerminalAction() {
        if (identityIndex(actionTarget) < 0 || studySession.busy) return
        if (actionKind === "close") studySession.closeTerminal(actionTarget)
        else { const pane = panel(identityIndex(actionTarget)); studySession.reconnect(pane.terminal, actionTarget, true); terminalAction.close() }
    }
    property bool agentsOnly: false
    property bool needsOnly: false
    readonly property bool overviewFiltered: agentsOnly || needsOnly
    property var overviewGroups: []
    property var overviewOrdinals: []
    property int overviewMatches: 0
    property int waitingCount: 0
    readonly property var overviewGroupTops: { let tops = []; for (let w = 0; w < spaces.length; ++w) tops.push(deck.y + deck.groupTop(w) - deck.contentY); return tops }
    function rebuildOverview() {
        if (dragSlot >= 0) return
        let groups = [], positions = [], total = 0, waiting = 0
        for (let i = 0; i < terminalModel.count; ++i) positions.push(-1)
        for (let w = 0; w < spaces.length; ++w) {
            let matches = []
            for (let i of indices(w)) {
                const pane = panel(i), facts = pane ? pane.facts : ({agent: "", needs_input: false})
                if (facts.needs_input) ++waiting
                if (overview && ((agentsOnly && !facts.agent) || (needsOnly && !facts.needs_input))) continue
                positions[i] = matches.length; matches.push(i); ++total
            }
            groups.push(matches)
        }
        overviewGroups = groups; overviewOrdinals = positions; overviewMatches = total; waitingCount = waiting
        if (overview) deck.contentY = Math.max(0, Math.min(deck.contentY, deck.contentHeight - deck.height))
    }
    function overviewIndices(space) { return overviewGroups[space] || [] }
    function changeFilter(agents, needs) {
        cancelDrag(); agentsOnly = agents; needsOnly = needs; rebuildOverview(); deck.contentY = 0
    }
    onOverviewChanged: { Qt.callLater(rebuildOverview); scheduleViewSave() }
    function openAgents() { if (!overview) toggleOverview(); changeFilter(true, false) }
    WorkbenchActions { id: commandRegistry; host: window; session: studySession }
    readonly property bool shortcutSettingsOpen: shortcutSettings.visible
    readonly property bool shortcutRecording: shortcutSettings.recordingActive
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
        if (shortcutSettings.visible || fontSettings.visible || terminalAction.visible || createDialog.visible || appearance.visible || commands.visible || help.visible) return
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
        if (terminalAction.visible || createDialog.visible || appearance.visible || help.visible || drawer.visible || paneMenu.visible || shortcutSettings.visible || fontSettings.visible || commands.visible || dragSlot >= 0) return
        if (appMenu.visible) { pendingCommands = true; appMenu.close() }
        else showCommands()
    }
    function showCommands() {
        commandTerminal = selected >= 0 ? terminalModel.get(selected).terminalId : ""
        commandWorkspace = spaceIds[workspace]
        commands.open()
    }
    function dispatchCommand(id) {
        const entry = commandRegistry.find(id)
        if (!entry || !entry.available) return
        switch (id) {
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
        case "close-terminal": requestTerminalAction(terminalModel.get(selected).terminalId, "close"); break
        case "forward": panel(selected).terminal.forceActiveFocus(); panel(selected).terminal.forwardNextKey(); break;
        case "copy": panel(selected).terminal.copy(); break
        case "paste": panel(selected).terminal.paste(); break
        case "motion": reducedMotion = !reducedMotion; break
        case "help": help.open(); break
        }
    }
    property bool reducedMotion: false
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
    property int previewTheme: 0
    readonly property bool appearanceOpen: appearance.visible
    readonly property bool themeHelpOpen: appearance.helpOpen
    readonly property bool applicationMenuOpen: appMenu.visible
    readonly property bool layoutMenuOpen: paneMenu.visible
    readonly property bool themeSearchFocus: appearance.searchFocused
    readonly property int themeMatchCount: appearance.matches.length
    readonly property var chrome: studyThemes.theme(themeIndexFor(workspace))
    function themeIndexFor(space) { return appearance.visible && spaceIds[space] === appearanceWorkspace ? previewTheme : (workspaceThemes[space] || 0) }
    function openAppearance() {
        if (terminalAction.visible || overview || createDialog.visible || appearance.visible) return
        if (appMenu.visible) { pendingAppearance = true; appMenu.close() }
        else showAppearance()
    }
    function showAppearance() {
        studyThemes.reloadDirectory()
        appearanceWorkspace = spaceIds[workspace]; previewTheme = workspaceThemes[workspace] || 0; appearance.open()
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
    readonly property bool canCreateTerminal: spaces.length > 0 && counts[workspace] < 8 && !studySession.busy && dragSlot < 0
    readonly property bool canCreateWorkspace: spaces.length < 8 && !studySession.busy && dragSlot < 0
    ListModel { id: terminalModel }
    property int catalogRevision: 0
    function indices(space) { const revision = catalogRevision; let result = []; for (let i = 0; i < terminalModel.count; ++i) if (terminalModel.get(i).spaceIndex === space) result.push(i); return result.sort((a,b) => terminalModel.get(a).ordinal - terminalModel.get(b).ordinal) }
    function ordinal(i) { const revision = catalogRevision; return i >= 0 && i < terminalModel.count ? terminalModel.get(i).ordinal : -1 }
    function applyCatalog(catalog, method) {
        if (method === "catalog.update") { cancelDrag(); paneMenu.close() }
        const selectedId = selected >= 0 && selected < terminalModel.count ? terminalModel.get(selected).terminalId : ""
        const oldWorkspace = workspace
        let previous = {}
        for (let w = 0; w < spaceIds.length; ++w) {
            const i = remembered[w]
            previous[spaceIds[w]] = {focus: i >= 0 && i < terminalModel.count ? terminalModel.get(i).terminalId : "", x: positions[w] || 0, theme: workspaceThemes[w] || 0}
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
            names.push(space.name); ids.push(space.id); folders.push(space.directory || studySession.defaultDirectory); totals.push(space.terminals.length)
            const retained = previous[space.id]
            focus.push(retained ? identityIndex(retained.focus) : -1); scroll.push(retained ? retained.x : 0); themes.push(retained ? retained.theme : 0)
            for (let j = 0; j < space.terminals.length; ++j) {
                const terminal = space.terminals[j]
                let existing = -1
                for (let k = 0; k < terminalModel.count; ++k) if (terminalModel.get(k).terminalId === terminal.id) { existing = k; break }
                if (existing >= 0) {
                    terminalModel.setProperty(existing, "spaceIndex", w)
                    terminalModel.setProperty(existing, "ordinal", j)
                    continue
                }
                sizes.push(0.5); full.push(false); widths = sizes; fullWidths = full
                terminalModel.append({terminalId: terminal.id, spaceIndex: w, ordinal: j})
                added = terminalModel.count - 1
                if (focus[w] < 0) focus[w] = added
            }
        }
        spaces = names; spaceIds = ids; directories = folders; counts = totals; workspaceThemes = themes
        ++catalogRevision
        Qt.callLater(rebuildOverview)
        for (let w = 0; w < names.length; ++w) {
            const items = indices(w)
            if (items.indexOf(focus[w]) < 0) focus[w] = items.length ? items[0] : -1
        }
        remembered = focus; positions = scroll
        selected = identityIndex(selectedId)
        if (selected < 0) { workspace = Math.min(oldWorkspace, names.length - 1); selected = focus[workspace] ?? -1 }
        Qt.callLater(syncItems)
        if (selected >= 0) {
            workspace = terminalModel.get(selected).spaceIndex
            let values = remembered.slice(); values[workspace] = selected; remembered = values
            if (!overview) Qt.callLater(reveal)
        }
        if (terminalAction.visible && identityIndex(actionTarget) < 0) terminalAction.close()
        if (createDialog.visible && workspaceFormMode !== "create" && spaceIds.indexOf(editingWorkspace) < 0) createDialog.close()
        if (appearance.visible && spaceIds.indexOf(appearanceWorkspace) < 0) appearance.close()
        if (method === "workspace.create_at") { createDialog.close(); chooseWorkspace(spaces.length - 1); if (overview) toggleOverview() }
        if (method === "terminal.move" && followMove && selected >= 0) {
            followMove = false
            Qt.callLater(function() { if (overview) revealOverview(selected); else focusPane(selected) })
        }
        if (method === "terminal.close") {
            terminalAction.close(); Qt.callLater(restoreAfterPopup)
        }
        if (method === "workspace.rename" || method === "workspace.directory") createDialog.close()
        if (method === "terminal.create" && added >= 0) { if (overview) toggleOverview(); Qt.callLater(function() { focusPane(added) }) }
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
        newFolder.text = directories[target] || studySession.defaultDirectory
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
        else studySession.createWorkspace(newName.text, newFolder.text)
    }
    property bool pendingForward: false
    readonly property bool forwardingArmed: selected >= 0 && panel(selected) !== null && panel(selected).terminal.forwardingArmed
    function restoreAfterPopup() {
        if (pendingForward) { pendingForward = false; Qt.callLater(function() { window.dispatchCommand("forward") }); return }
        if (pendingShortcutSettings) {
            pendingShortcutSettings = false; Qt.callLater(function() { studyShortcuts.begin(); shortcutSettings.open() })
        } else if (pendingFontSettings) {
            pendingFontSettings = false; Qt.callLater(function() { studyFonts.begin(); fontSettings.open() })
        } else if (pendingCommands) {
            pendingCommands = false; Qt.callLater(showCommands)
        } else if (pendingAppearance) {
            pendingAppearance = false; Qt.callLater(showAppearance)
        } else if (pendingWorkspaceDialog) {
            pendingWorkspaceDialog = false; Qt.callLater(openWorkspaceForm)
        } else if (!overview && selected >= 0 && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen && !appearance.visible && !help.visible && !shortcutSettings.visible && !fontSettings.visible && !commands.visible) panel(selected).terminal.forceActiveFocus()
    }
    Connections {
        target: studySession
        function onCompleted(catalog, method) { window.applyCatalog(catalog, method) }
        function onActionCompleted(identity, method) {
            if (method === "terminal.recover") { const i = window.identityIndex(identity); if (i >= 0) studySession.reconnect(window.panel(i).terminal, identity, false) }
        }
        function onChanged() { if (!studySession.busy && studySession.error.length > 0) window.followMove = false }
    }
    readonly property color accent: chrome.accent
    readonly property int motion: reducedMotion ? 0 : 220
    function panel(i) { return panels.itemAt(i) }
    function paneWidth(i) { return Math.max(240, (deck.width + 12) * (fullWidths[i] ? 1 : widths[i]) - 12) }
    function left(i) { if (i < 0) return 0; let x = 0; const items = indices(terminalModel.get(i).spaceIndex); for (let j of items) { if (j === i) break; x += paneWidth(j) + 12 } return x }
    function stripWidth() { const items = indices(workspace); return items.length === 0 ? deck.width : Math.max(deck.width, left(items[items.length - 1]) + paneWidth(items[items.length - 1])) }
    function reveal() {
        if (overview || selected < 0) return
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
        if (!overview) { reveal(); panel(i).terminal.forceActiveFocus() }
    }
    function chooseWorkspace(i) {
        let values = positions.slice(); values[workspace] = deck.contentX; positions = values
        workspace = Math.max(0, Math.min(spaces.length - 1, i)); selected = remembered[workspace] === undefined ? -1 : remembered[workspace]
        drawer.close(); deck.scrollTarget = overview ? 0 : Math.max(0, Math.min(positions[workspace] || 0, stripWidth() - deck.width)); deck.contentX = deck.scrollTarget
        if (!overview && selected >= 0) Qt.callLater(function() { reveal(); panel(selected).terminal.forceActiveFocus() })
        else window.contentItem.forceActiveFocus()
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
            const cellWidth = (deck.width + 12) / 3
            const col = Math.max(0, Math.min(2, Math.floor(point.x / cellWidth)))
            const items = overviewIndices(w), slot = items[row * 3 + col]
            const half = slot === undefined ? cellWidth / 2 : panel(slot).width * panel(slot).previewScale / 2
            const projected = Math.min(items.length, row * 3 + col + (point.x - col * cellWidth >= half ? 1 : 0))
            const raw = projected >= items.length ? (items.length ? terminalModel.get(items[items.length - 1]).ordinal + 1 : 0) : terminalModel.get(items[projected]).ordinal
            const source = terminalModel.get(dragSlot)
            dropRaw = raw; dropProjected = projected
            dropPosition = raw - (source.spaceIndex === w && source.ordinal < raw ? 1 : 0)
            dropAllowed = source.spaceIndex === w || counts[w] < 8
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
        if (overview) window.contentItem.forceActiveFocus()
        else if (selected >= 0) Qt.callLater(function() { reveal(); panel(selected).terminal.forceActiveFocus() })
    }
    function enterPane(i) { focusPane(i); if (overview) toggleOverview() }
    function navigate(delta) {
        const items = overview ? overviewIndices(workspace) : indices(workspace); if (items.length) focusPane(items[Math.max(0, Math.min(items.length - 1, items.indexOf(selected) + delta))])
    }
    readonly property bool moveKeyContext: !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalAction.visible && !createDialog.visible && !appearance.visible && !help.visible && !drawer.visible && !appMenu.visible && !paneMenu.visible && dragSlot < 0
    property string moveNotice: ""
    property bool followMove: false
    function canReorder(delta) {
        if (selected < 0 || studySession.busy || (overview && overviewOrdinals[selected] < 0)) return false
        const source = terminalModel.get(selected), count = counts[source.spaceIndex]
        return source.ordinal + delta >= 0 && source.ordinal + delta < count
    }
    function reorderSelected(delta) {
        if (!canReorder(delta)) return
        moveNotice = ""
        const source = terminalModel.get(selected)
        followMove = true
        studySession.moveTerminal(source.terminalId, spaceIds[source.spaceIndex], source.ordinal + delta)
    }
    function canMoveWorkspace(delta) {
        const target = workspace + delta
        return selected >= 0 && (!overview || overviewOrdinals[selected] >= 0) && !studySession.busy && target >= 0 && target < spaces.length && counts[target] < 8
    }
    function moveSelectedWorkspace(delta) {
        if (selected < 0 || studySession.busy || (overview && overviewOrdinals[selected] < 0)) return
        const target = workspace + delta
        if (target < 0 || target >= spaces.length) return
        if (counts[target] >= 8) {
            moveNotice = spaces[target] + " already has eight terminals"; moveNoticeTimer.restart(); return
        }
        moveNotice = ""
        followMove = true
        studySession.moveTerminal(terminalModel.get(selected).terminalId, spaceIds[target], -1)
    }
    function revealOverview(i) {
        const source = terminalModel.get(i)
        if (overviewOrdinals[i] < 0) return
        const top = deck.groupTop(source.spaceIndex) + Math.floor(overviewOrdinals[i] / 3) * deck.rowStride + deck.previewTop
        if (top < deck.contentY) deck.contentY = top
        else if (top + deck.previewHeight > deck.contentY + deck.height) deck.contentY = top + deck.previewHeight - deck.height
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
        topPadding: 5; bottomPadding: 5; leftPadding: 8; rightPadding: 8
        contentItem: Rectangle { implicitWidth: 248; implicitHeight: 1; color: window.chrome.border }
    }
    Rectangle { x: 0; y: 0; width: parent.width; height: 40; color: window.chrome.surface }
    Rectangle { x: 0; y: 40; width: 44; height: parent.height - 40; color: window.chrome.surface }
    QuietButton { objectName: "applicationMenuButton"; x: 8; y: 6; height: 28; iconName: "menu"; hint: "Application menu"; selected: appMenu.visible; onClicked: appMenu.open() }
    Column {
        x: 8; y: 52; spacing: 14
        QuietButton { objectName: "workspacesButton"; height: 28; iconName: "workspaces"; hint: "Workspaces"; selected: drawer.visible; onClicked: drawer.open() }
        QuietButton { objectName: "agentsButton"; height: 28; iconName: "agents"; hint: "Agents"; selected: window.overview && window.agentsOnly; onClicked: window.openAgents() }
    }
    Rectangle {
        x: 27; y: 87; width: 16; height: 16; radius: 8; visible: window.waitingCount > 0; color: window.chrome.waiting
        Text { anchors.centerIn: parent; text: window.waitingCount; color: window.chrome.background; font.pixelSize: 10 }
    }
    QuietButton { x: 8; height: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; iconName: "help"; hint: "Keyboard shortcuts"; onClicked: help.open() }
    Item {
        id: topbar; x: 44; y: 4; width: parent.width - 56; height: 32
        QuietButton { id: workspaceButton; objectName: "workspaceButton"; leftAligned: true; width: Math.min(200, Math.max(80, topbar.width / 4)); text: window.overview ? "Overview" : (window.spaces[window.workspace] || ""); hint: "Choose workspace"; onClicked: drawer.open() }
        Rectangle {
            anchors.centerIn: parent; width: indicatorRow.width + 8; height: 28; radius: 14; color: window.chrome.background
            visible: !window.overview && window.counts[window.workspace] > 0
        }
        Row {
            id: indicatorRow; objectName: "indicatorRow"; anchors.centerIn: parent; spacing: 0; visible: !window.overview && window.counts[window.workspace] > 0
            Repeater {
                model: window.counts[window.workspace] || 0
                QuietButton {
                    required property int index
                    implicitWidth: 28; implicitHeight: 28; text: ""; hint: { const item = paneIndex < 0 ? null : window.panel(paneIndex); return item ? item.facts.title : "Terminal" }
                    readonly property int paneIndex: window.indices(window.workspace)[index] ?? -1
                    Rectangle {
                        anchors.centerIn: parent; width: window.selected === parent.paneIndex ? 24 : 14; height: 8; radius: 4
                        color: window.selected === parent.paneIndex ? window.accent : window.left(parent.paneIndex) + window.paneWidth(parent.paneIndex) > deck.contentX && window.left(parent.paneIndex) < deck.contentX + deck.width ? window.chrome.muted : window.chrome.border
                    }
                    Rectangle {
                        x: parent.width - 5; y: 1; width: 4; height: 4; radius: 2; color: window.chrome.waiting
                        visible: { const item = parent.paneIndex < 0 ? null : window.panel(parent.paneIndex); return item !== null && item.facts.needs_input }
                    }
                    onClicked: window.enterPane(paneIndex)
                }
            }
        }
        Label {
            id: focusedCount; objectName: "focusedCount"
            anchors.left: indicatorRow.right; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
            visible: !window.overview && window.counts[window.workspace] > 0
            text: (window.selected < 0 ? 0 : window.ordinal(window.selected) + 1) + " / " + (window.counts[window.workspace] || 0)
            color: window.chrome.muted; font.pixelSize: 12
        }
        QuietButton {
            objectName: "newTerminalButton"; iconName: "plus"; hint: "New terminal" + (studyShortcuts.labels["new-terminal"] ? " · " + studyShortcuts.labels["new-terminal"] : ""); enabled: window.canCreateTerminal
            x: window.overview ? overviewButton.x - width - 12 : focusedCount.x + focusedCount.width + 12
            onClicked: window.newTerminal()
        }
        QuietButton { id: overviewButton; objectName: "overviewButton"; anchors.right: parent.right; text: window.overview ? "Done" : "Overview"; onClicked: window.toggleOverview() }
    }
    Row {
        x: 60; y: 54; spacing: 8; visible: window.overview
        QuietButton { objectName: "overviewAgentFilter"; hint: "Filter terminal or agent identity"; text: window.agentsOnly ? "Show: Agents" : "Show: All terminals"; onClicked: window.changeFilter(!window.agentsOnly, window.needsOnly); background: Rectangle { radius: 6; color: window.agentsOnly ? window.chrome.surface : "transparent"; border.width: window.agentsOnly ? 1 : 0; border.color: window.accent } }
        QuietButton { objectName: "overviewStateFilter"; hint: "Filter agents that need input"; text: window.needsOnly ? "State: Needs input" : "State: All states"; onClicked: window.changeFilter(window.agentsOnly, !window.needsOnly); background: Rectangle { radius: 6; color: window.needsOnly ? window.chrome.surface : "transparent"; border.width: window.needsOnly ? 1 : 0; border.color: window.accent } }
    }
    Flickable {
        id: deck; objectName: "deck"
        onContentXChanged: window.scheduleViewSave()
        onContentYChanged: window.scheduleViewSave()
        property real scrollTarget: 0
        x: 60; y: window.overview ? 102 : 60; width: parent.width - 72; height: parent.height - y - 12
        clip: true; boundsBehavior: Flickable.StopAtBounds
        interactive: window.overview && window.dragSlot < 0
        contentWidth: window.overview ? width : window.stripWidth()
        contentHeight: window.overview ? overviewHeight() : height
        readonly property real previewHeight: Math.min(240, Math.max(150, (height - 100) / 2))
        readonly property real previewTop: 52
        readonly property real rowStride: previewHeight + 34
        function rows(space) { return Math.max(1, Math.ceil(window.overviewIndices(space).length / 3)) }
        function groupTop(space) { let y = 0; for (let i = 0; i < space; ++i) if (!window.overviewFiltered || window.overviewIndices(i).length) y += rows(i) * rowStride + 64; return y }
        function overviewHeight() { return Math.max(height, groupTop(window.spaces.length)) }
        Behavior on contentX { enabled: !window.overview; NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
        Repeater {
            model: window.spaces.length
            Label { required property int index; visible: window.overview && (!window.overviewFiltered || window.overviewIndices(index).length > 0); x: 0; y: deck.groupTop(index); text: window.spaces[index] + (window.overviewIndices(index).length > 0 ? "  ·  " + window.overviewIndices(index).length + (window.overviewIndices(index).length === 1 ? " terminal" : " terminals") : ""); color: window.chrome.muted; font.pixelSize: 13 }
        }
        Repeater {
            id: panels; model: terminalModel
            delegate: Rectangle {
                id: pane; required property int index
                required property int spaceIndex
                required property int ordinal
                required property string terminalId
                Connections { target: pane.runtimeFacts; function onChanged() { Qt.callLater(window.rebuildOverview) } }
                property alias terminal: terminal
                readonly property var runtimeFacts: studySession.observe(terminalId)
                readonly property var facts: runtimeFacts ? runtimeFacts.facts : ({title: "Terminal", agent: "", state: ""})
                opacity: window.dragSlot === index ? 0.45 : 1
                readonly property int overviewOrdinal: window.overviewOrdinals[index] ?? -1
                readonly property real previewScale: Math.min((deck.width - 24) / 3 / width, deck.previewHeight / height)
                objectName: "pane" + index
                width: window.paneWidth(index); height: window.height - 72
                x: window.overview ? (overviewOrdinal % 3) * (deck.width + 12) / 3 : window.left(index)
                y: window.overview ? deck.groupTop(spaceIndex) + Math.floor(overviewOrdinal / 3) * deck.rowStride + deck.previewTop : 0
                scale: window.overview ? previewScale : 1
                transformOrigin: Item.TopLeft
                visible: window.overview ? overviewOrdinal >= 0 : spaceIndex === window.workspace
                color: studyThemes.theme(window.themeIndexFor(spaceIndex)).surface; radius: 6
                border.width: 1; border.color: window.selected === index ? studyThemes.theme(window.themeIndexFor(spaceIndex)).accent : studyThemes.theme(window.themeIndexFor(spaceIndex)).border
                Behavior on x { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Text { x: 14; y: 10; width: parent.width - 65 - headerState.width - (headerState.width > 0 ? 12 : 0); elide: Text.ElideMiddle; text: pane.facts.title; textFormat: Text.PlainText; font.pixelSize: 12; color: studyThemes.theme(window.themeIndexFor(spaceIndex)).foreground }
                Text {
                    id: headerState; x: parent.width - 48 - width; y: 10; width: Math.min(implicitWidth, parent.width * 0.45)
                    text: pane.facts.agent ? pane.facts.agent + " · " + pane.facts.state : ""; textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: 12
                    color: pane.facts.needs_input ? studyThemes.theme(window.themeIndexFor(spaceIndex)).waiting : pane.facts.state === "Ready" ? studyThemes.theme(window.themeIndexFor(spaceIndex)).ready : studyThemes.theme(window.themeIndexFor(spaceIndex)).muted
                }
                QuietButton { objectName: "paneLayoutButton"; anchors.right: parent.right; anchors.rightMargin: 3; y: 0; iconName: "more"; height: 28; hint: "Terminal layout"; enabled: !window.overview; onClicked: { window.focusPane(index); window.paneMenuTarget = pane.terminalId; const point = mapToItem(window.contentItem, 0, height); paneMenu.x = Math.max(0, Math.min(point.x, window.width - paneMenu.width - 12)); paneMenu.y = point.y; paneMenu.open() } }
                TerminalItem {
                    id: terminal; objectName: "terminal" + index
                    property int workspaceIndex: pane.spaceIndex
                    property int orderInWorkspace: pane.ordinal
                    property string stableIdentity: pane.terminalId
                    property var runtimeFacts: pane.facts
                    x: 4; y: 36; width: parent.width - 8; height: parent.height - 40
                    enabled: !window.overview
                    textFont: studyFonts.current
                    themeIndex: window.themeIndexFor(pane.spaceIndex)
                    presentationEnabled: pane.visible && pane.x + pane.width * pane.scale > deck.contentX && pane.x < deck.contentX + deck.width && pane.y + pane.height * pane.scale > deck.contentY && pane.y < deck.contentY + deck.height
                    opacity: presentationEnabled ? 1 : 0
                    WheelHandler {
                        orientation: Qt.Horizontal
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        target: null
                        onWheel: event => {
                            const dx = event.pixelDelta.x || event.angleDelta.x
                            if (!window.overview && dx !== 0) {
                                deck.scrollTarget = Math.max(0, Math.min(deck.contentWidth - deck.width, deck.contentX - dx)); deck.contentX = deck.scrollTarget; event.accepted = true
                            } else event.accepted = false
                        }
                    }
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
                readonly property var lifecycle: studySession.lifecycle[terminalId] || ({state: "unavailable", message: "Checking terminal status…", busy: false})
                Rectangle {
                    anchors.centerIn: terminal; width: Math.min(parent.width - 40, 420); height: lifecycleBody.implicitHeight + 32; radius: 10
                    visible: !terminal.ready; color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).raised; border.color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).border
                    Column {
                        id: lifecycleBody; x: 16; y: 16; width: parent.width - 32; spacing: 10
                        Label {
                            width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 14
                            color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).foreground
                            text: terminal.attachment === "connecting" ? "Connecting…" : terminal.attachment === "elsewhere" ? "Open in another window" : (pane.lifecycle.message || "Checking terminal status…")
                        }
                        Label {
                            width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 12
                            color: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)).muted
                            text: pane.lifecycle.state === "stopped" ? "Start a new shell in this workspace’s folder. The previous process has ended." : terminal.attachment === "elsewhere" ? "Move this terminal here to continue with the same process." : "Reconnect keeps a running shell. Check again if its status has changed."
                            visible: terminal.attachment !== "connecting"
                        }
                        Flow {
                            width: parent.width; spacing: 8; visible: terminal.attachment !== "connecting"
                            QuietButton {
                                objectName: "terminalPrimary" + pane.index; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex))
                                text: pane.lifecycle.state === "stopped" ? "Start new shell" : terminal.attachment === "elsewhere" ? "Move here…" : "Reconnect"
                                visible: pane.lifecycle.state === "stopped" || pane.lifecycle.state === "running"
                                enabled: !studySession.busy && !pane.lifecycle.busy && !window.overview
                                onClicked: { if (pane.lifecycle.state === "stopped") studySession.recover(pane.terminalId); else if (terminal.attachment === "elsewhere") window.requestTerminalAction(pane.terminalId, "move"); else studySession.reconnect(terminal, pane.terminalId, false) }
                            }
                            QuietButton { objectName: "terminalReconnect" + pane.index; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)); text: "Reconnect"; visible: terminal.attachment === "elsewhere" && pane.lifecycle.state === "running"; enabled: !studySession.busy && !pane.lifecycle.busy && !window.overview; onClicked: studySession.reconnect(terminal, pane.terminalId, false) }
                            QuietButton { objectName: "terminalCheck" + pane.index; tones: studyThemes.theme(window.themeIndexFor(pane.spaceIndex)); text: pane.lifecycle.busy ? "Checking…" : "Check again"; enabled: !pane.lifecycle.busy && !window.overview; onClicked: studySession.inspect(pane.terminalId) }
                        }
                    }
                }
                MouseArea {
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
            Label {
                required property int index
                objectName: "overviewCaption" + index
                Component.onCompleted: { let labels = window.overviewLabels.slice(); labels[index] = this; window.overviewLabels = labels }
                readonly property var pane: { const ready = window.terminalItems.length; return panels.itemAt(index) }
                visible: window.overview && pane !== null && pane.visible
                x: pane ? pane.x : 0; y: pane ? pane.y - 22 : 0
                width: (deck.width - 24) / 3; height: 18
                text: pane ? pane.facts.title + (pane.facts.agent ? " · " + pane.facts.agent + " · " + pane.facts.state : "") : ""
                textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: 12
                color: pane && pane.facts.needs_input ? window.chrome.waiting : window.chrome.foreground
            }
        }
        Label { anchors.centerIn: parent; visible: window.overview && window.overviewFiltered && window.overviewMatches === 0; text: window.needsOnly ? "No agents need input" : "No agents detected"; color: window.chrome.muted; font.pixelSize: 14 }
        Label { anchors.centerIn: parent; visible: !window.overview && (window.counts[window.workspace] || 0) === 0; text: "An empty workspace"; color: window.chrome.muted; font.pixelSize: 14 }
    }
    Item {
        x: deck.x; y: deck.y; width: deck.width; height: deck.height; clip: true
        Rectangle {
            visible: window.dragSlot >= 0 && window.dropSpace >= 0
            x: 0; y: deck.groupTop(window.dropSpace) - deck.contentY
            width: deck.width; height: deck.groupTop(window.dropSpace + 1) - deck.groupTop(window.dropSpace) - 8
            color: "transparent"; radius: 8; border.width: 1
            border.color: window.dropAllowed ? window.accent : "#e49b9b"
        }
        Rectangle {
            visible: window.dragSlot >= 0 && window.dropSpace >= 0 && window.dropAllowed
            readonly property int total: window.overviewIndices(window.dropSpace).length
            readonly property int before: Math.min(window.dropProjected, Math.max(0, total - 1))
            readonly property int slot: window.dropSpace < 0 ? -1 : (window.overviewIndices(window.dropSpace)[before] ?? -1)
            x: slot < 0 ? 6 : (before % 3) * (deck.width + 12) / 3 + (window.dropProjected === total ? window.panel(slot).width * window.panel(slot).previewScale + 5 : -5)
            y: deck.groupTop(window.dropSpace) + Math.floor(before / 3) * deck.rowStride + deck.previewTop - deck.contentY
            width: 3; height: deck.previewHeight; radius: 1.5; color: window.accent
        }
    }
    Rectangle {
        visible: window.dragSlot >= 0
        x: Math.min(window.width - width - 12, deck.x + window.dragPoint.x + 16)
        y: Math.max(60, Math.min(window.height - height - 12, deck.y + window.dragPoint.y + 16))
        width: dragLabel.implicitWidth + 24; height: 38; radius: 8; color: window.chrome.surface; border.color: window.accent; z: 5
        Label { id: dragLabel; anchors.centerIn: parent; text: (window.dropSpace < 0 ? "Move terminal" : !window.dropAllowed ? "Workspace is full" : window.spaces[window.dropSpace] + " · position " + (window.dropPosition + 1)) + " · Esc cancels"; color: window.chrome.foreground }
    }
    Timer {
        interval: 16; repeat: true; running: window.dragSlot >= 0
        onTriggered: {
            const y = window.dragPoint.y
            const delta = y < 56 ? -Math.min(12, (56 - y) / 5) : y > deck.height - 56 ? Math.min(12, (y - deck.height + 56) / 5) : 0
            if (delta && window.dragPoint.x >= 0 && window.dragPoint.x <= deck.width) { deck.contentY = Math.max(0, Math.min(deck.contentHeight - deck.height, deck.contentY + delta)); window.updateDrop(window.dragPoint) }
        }
    }
    Drawer {
        id: drawer; y: 40; width: 288; height: window.height - 40; edge: Qt.LeftEdge; leftPadding: 44
        background: Rectangle { color: window.chrome.surface; border.color: window.chrome.border }
        Column {
            anchors.fill: parent; anchors.margins: 14; spacing: 8
            Label { text: "Workspaces"; color: window.chrome.foreground; font.pixelSize: 16; topPadding: 6; bottomPadding: 12; leftPadding: 8 }
            Repeater { model: window.spaces.length; QuietButton { required property int index; width: parent.width; height: 46; leftAligned: true; selected: index === window.workspace; trailingText: String(window.counts[index]); text: window.spaces[index]; onClicked: window.chooseWorkspace(index) } }
            QuietButton { objectName: "newWorkspaceButton"; width: parent.width; leftAligned: true; text: "New workspace…"; enabled: window.canCreateWorkspace; onClicked: window.newWorkspace() }
            Label { width: parent.width; padding: 8; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: window.chrome.muted; font.pixelSize: 12 }
        }
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
        MenuAction { objectName: "renameWorkspaceMenuItem"; text: "Rename workspace…"; enabled: !window.overview && !studySession.busy; onTriggered: window.editWorkspace("rename") }
        MenuAction { objectName: "workspaceFolderMenuItem"; text: "Workspace folder…"; enabled: !window.overview && !studySession.busy; onTriggered: window.editWorkspace("folder") }
        MenuRule {}
        MenuAction { objectName: "appearanceMenuItem"; text: "Appearance…"; enabled: !window.overview; onTriggered: window.openAppearance() }
        MenuAction { objectName: "fontSettingsMenuItem"; text: "Terminal text…"; onTriggered: window.openFontSettings() }
        MenuAction { objectName: "shortcutSettingsMenuItem"; text: "Shortcut settings…"; onTriggered: window.openShortcutSettings() }
        MenuAction { keyHint: studyShortcuts.labels["help"] || ""; text: "Keyboard shortcuts"; onTriggered: help.open() }
        MenuAction { text: "Reduced motion"; checkable: true; checked: window.reducedMotion; onTriggered: window.reducedMotion = checked }
        MenuRule {}
        MenuAction { objectName: "forwardKeyMenuItem"; text: "Send next key to terminal"; keyHint: studyShortcuts.labels["forward"] || ""; enabled: commandRegistry.find("forward").available; onTriggered: { window.pendingForward = true; appMenu.close() } }
        MenuAction { text: "Copy selection"; enabled: window.selected >= 0; onTriggered: window.panel(window.selected).terminal.copy() }
        MenuAction { text: "Paste"; enabled: window.selected >= 0 && !window.overview; onTriggered: { window.panel(window.selected).terminal.paste(); window.panel(window.selected).terminal.forceActiveFocus() } }
        MenuRule {}
        MenuAction { text: studySession.savedSession ? "Refresh workspaces" : "Refresh study workspaces"; enabled: !studySession.busy; onTriggered: studySession.refresh() }
        MenuAction { text: studySession.savedSession ? "Close window" : "Close UI study"; onTriggered: window.close() }
    }
    ChromeMenu {
        tones: window.chrome
        id: paneMenu
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
        MenuAction { objectName: "terminalCloseMenuItem"; text: "Close terminal…"; enabled: window.selected >= 0 && !studySession.busy; onTriggered: window.requestTerminalAction(window.paneMenuTarget, "close") }
        MenuRule {}
        MenuAction { text: "Cycle width"; onTriggered: window.dispatchCommand("cycle-width") }
        MenuAction { text: "Toggle full width"; onTriggered: window.dispatchCommand("full-width") }
        MenuRule {}
        MenuAction { objectName: "moveLeftMenuItem"; text: "Move terminal left"; enabled: window.canReorder(-1); onTriggered: window.reorderSelected(-1) }
        MenuAction { objectName: "moveRightMenuItem"; text: "Move terminal right"; enabled: window.canReorder(1); onTriggered: window.reorderSelected(1) }
        MenuRule {}
        MenuAction { text: "Move to workspace above"; enabled: window.canMoveWorkspace(-1); onTriggered: window.moveSelectedWorkspace(-1) }
        MenuAction { text: "Move to workspace below"; enabled: window.canMoveWorkspace(1); onTriggered: window.moveSelectedWorkspace(1) }
    }
    Rectangle {
        visible: window.moveNotice.length > 0 && studySession.error.length === 0 && window.moveKeyContext
        anchors.bottom: parent.bottom; anchors.bottomMargin: 16; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 100, moveNoticeText.implicitWidth + 32); height: 42; radius: 8; z: 4
        color: window.chrome.surface; border.color: window.chrome.border
        Label { id: moveNoticeText; anchors.centerIn: parent; text: window.moveNotice; color: window.chrome.foreground; font.pixelSize: 13 }
    }
    Timer { id: moveNoticeTimer; interval: 3000; onTriggered: window.moveNotice = "" }
    Rectangle {
        visible: (studySession.error.length > 0 || studySession.catalogNotice.length > 0) && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(window.width - 100, 680); height: 76; color: "#352b22"; radius: 8; z: 4
        Label { x: 16; y: 12; width: parent.width - 120; text: studySession.error || studySession.catalogNotice; wrapMode: Text.Wrap; color: "#f0d6b8" }
        QuietButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Refresh"; enabled: !studySession.busy; onClicked: studySession.refresh() }
    }
    ThemePicker {
        id: appearance; anchors.centerIn: parent
        retainedTheme: studyThemes.theme(window.previewTheme)
        directoryPath: studyThemes.themesPath; directoryStatus: studyThemes.directoryStatus
        onFolderRequested: studyThemes.openThemesDirectory()
        catalog: studyThemes.catalog; themeIndex: window.previewTheme
        workspaceName: window.spaces[window.spaceIds.indexOf(window.appearanceWorkspace)] || ""
        onPreviewSelected: index => window.previewTheme = index
        onApplySelected: {
            const space = window.spaceIds.indexOf(window.appearanceWorkspace)
            if (space >= 0) { let themes = window.workspaceThemes.slice(); themes[space] = window.previewTheme; window.workspaceThemes = themes }
            close()
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    FontSettings {
        id: fontSettings; anchors.centerIn: parent; tones: window.chrome; preferences: studyFonts
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    Rectangle {
        visible: window.forwardingArmed
        x: 60; y: 44; width: Math.min(520, window.width - 76); height: 44; radius: 6; z: 7
        color: window.chrome.raised; border.color: window.chrome.accent
        Label { anchors.fill: parent; anchors.margins: 10; anchors.rightMargin: 85; text: "Next key goes to this terminal · Escape cancels"; color: window.chrome.foreground; elide: Text.ElideRight }
        QuietButton { objectName: "cancelForwardKey"; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: "Cancel"; onClicked: if (window.selected >= 0) window.panel(window.selected).terminal.cancelForwarding() }
    }
    Connections { target: studyView; function onChanged() { window.fontWarningDismissed = false } }
    property bool fontWarningDismissed: false
    Rectangle {
        visible: (studyFonts.error.length > 0 || studyShortcuts.error.length > 0 || studyView.error.length > 0) && !window.fontWarningDismissed; x: 60; y: 44; width: Math.min(600, window.width - 76); height: 56; radius: 6; z: 6; color: window.chrome.raised
        QuietButton { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: "Dismiss"; onClicked: window.fontWarningDismissed = true }
        Label { anchors.fill: parent; anchors.margins: 10; anchors.rightMargin: 94; color: window.chrome.waiting; wrapMode: Text.Wrap; textFormat: Text.PlainText; text: [studyFonts.error, studyShortcuts.error, studyView.error].filter(value => value.length > 0).join("\n") }
    }
    CommandPalette {
        id: commands; objectName: "commandPalette"; anchors.centerIn: parent
        tones: window.chrome; actions: commandRegistry.entries
        contextLabel: window.spaces[window.workspace] + (window.selected >= 0 && window.panel(window.selected) ? " · " + window.panel(window.selected).facts.title : " · Empty workspace")
        onChosen: id => { window.pendingCommand = id; close() }
        onClosed: {
            const action = window.pendingCommand; window.pendingCommand = ""
            // Reject stale context rather than applying an action to a different pane.
            const targetWorkspace = window.commandWorkspace, targetTerminal = window.commandTerminal
            Qt.callLater(function() {
                const sameContext = targetWorkspace === window.spaceIds[window.workspace] && targetTerminal === (window.selected >= 0 ? terminalModel.get(window.selected).terminalId : "")
                if (action && sameContext) window.dispatchCommand(action)
                window.restoreAfterPopup()
            })
        }
    }
    Dialog {
        id: terminalAction; objectName: "terminalActionDialog"; anchors.centerIn: parent; modal: true
        width: Math.min(440, window.width - 64); padding: 20
        closePolicy: studySession.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
        title: window.actionKind === "close" ? "Close terminal?" : "Move terminal here?"
        background: Rectangle { color: window.chrome.raised; radius: 12; border.color: window.chrome.border }
        header: Label { text: terminalAction.title; padding: 20; bottomPadding: 8; color: window.chrome.foreground; font.pixelSize: 22 }
        contentItem: Column {
            spacing: 16
            Label { width: parent.width; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: window.chrome.muted; font.pixelSize: 12; text: window.actionContext }
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: window.chrome.foreground; text: window.actionKind === "close" ? "Stop this terminal’s process and remove it from the workspace. Other terminals keep running." : "Continue the same process in this window. The other window will release control." }
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: window.chrome.waiting; text: studySession.error; visible: text.length > 0 }
            Row {
                spacing: 8
                QuietButton { id: cancelTerminalAction; objectName: "cancelTerminalAction"; text: "Cancel"; enabled: !studySession.busy; onClicked: terminalAction.close() }
                QuietButton { objectName: "confirmTerminalAction"; text: studySession.busy ? "Working…" : window.actionKind === "close" ? "Close terminal" : "Move here"; enabled: !studySession.busy; onClicked: window.performTerminalAction() }
            }
        }
        onOpened: { studySession.clearError(); cancelTerminalAction.forceActiveFocus() }
        onClosed: Qt.callLater(window.restoreAfterPopup)
    }
    component ProjectField: TextField {
        selectByMouse: true; color: "#e0e6ef"; selectionColor: window.accent; selectedTextColor: "#101318"
        placeholderTextColor: "#7d899c"; padding: 10
        background: Rectangle { radius: 6; color: "#101318"; border.color: parent.activeFocus ? window.accent : "#3a4657" }
    }
    Dialog {
        id: createDialog; objectName: "createWorkspaceDialog"
        onOpened: {
            const field = window.workspaceFormMode === "folder" ? newFolder : newName
            field.forceActiveFocus(); field.deselect(); field.cursorPosition = field.text.length
        }
        implicitWidth: 460
        title: window.workspaceFormMode === "rename" ? "Rename workspace" : window.workspaceFormMode === "folder" ? "Workspace folder" : "New workspace"; anchors.centerIn: parent; modal: true; width: Math.min(460, window.width - 80)
        padding: 20
        background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
        header: Label { text: createDialog.title; padding: 20; bottomPadding: 6; color: "#e0e6ef"; font.pixelSize: 18 }
        closePolicy: studySession.busy || window.folderBrowserOpen ? Popup.NoAutoClose : Popup.CloseOnEscape
        contentItem: Column {
            spacing: 12
            Label { text: "Name"; visible: window.workspaceFormMode !== "folder"; color: "#c2c9d4" }
            ProjectField {
                id: newName; objectName: "workspaceNameInput"; width: parent.width; placeholderText: "Project name"
                visible: window.workspaceFormMode !== "folder"; enabled: !studySession.busy; maximumLength: 63
                onAccepted: { if (window.workspaceFormMode === "rename") { if (submitWorkspace.enabled) submitWorkspace.clicked() } else newFolder.forceActiveFocus() }
            }
            Label { text: "Folder"; visible: window.workspaceFormMode !== "rename"; color: "#c2c9d4" }
            Row {
                width: parent.width; spacing: 8; visible: window.workspaceFormMode !== "rename"
                ProjectField { id: newFolder; objectName: "workspaceFolderInput"; width: parent.width - browseFolder.width - parent.spacing; enabled: !studySession.busy; onAccepted: if (submitWorkspace.enabled) submitWorkspace.clicked() }
                QuietButton { id: browseFolder; objectName: "browseWorkspaceFolderButton"; fixedPalette: true; text: "Browse…"; hint: "Choose a folder"; enabled: !studySession.busy; onClicked: window.browseWorkspaceFolder() }
            }
            Label { width: parent.width; visible: window.workspaceFormMode !== "rename"; text: "Used by new terminals. Running terminals keep their current folders."; wrapMode: Text.Wrap; color: "#a5afbe"; font.pixelSize: 12 }
            Label { width: parent.width; text: studySession.error; visible: text.length > 0; wrapMode: Text.Wrap; color: "#e3bf91" }
            Row {
                spacing: 8
                QuietButton { fixedPalette: true; text: "Cancel"; enabled: !studySession.busy; onClicked: createDialog.close() }
                QuietButton {
                    id: submitWorkspace; objectName: "submitWorkspaceButton"; fixedPalette: true
                    text: studySession.busy ? (window.workspaceFormMode === "create" ? "Creating…" : "Saving…") : window.workspaceFormMode === "create" ? "Create" : "Save"
                    enabled: !studySession.busy && (window.workspaceFormMode === "folder" || newName.text.trim().length > 0) && (window.workspaceFormMode === "rename" || newFolder.text.trim().length > 0)
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
    Dialog {
        id: help; implicitWidth: 560; implicitHeight: 460; width: Math.min(implicitWidth, window.width - 64); height: Math.min(implicitHeight, window.height - 64); title: "Keyboard shortcuts"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
        padding: 16
        background: Rectangle { color: window.chrome.raised; radius: 12; border.color: window.chrome.border }
        header: Label { text: help.title; padding: 20; bottomPadding: 8; color: window.chrome.foreground; font.pixelSize: 22 }
        footer: Item {
            implicitHeight: 56
            QuietButton { x: 16; anchors.verticalCenter: parent.verticalCenter; text: "Edit shortcuts…"; onClicked: window.openShortcutSettings() }
            QuietButton { anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; text: "Close"; onClicked: help.close() }
        }
        contentItem: ListView {
            clip: true; spacing: 4
            model: commandRegistry.entries.filter(entry => entry.key.length > 0)
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                required property var modelData
                width: ListView.view.width; height: 34
                Label { x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.56; elide: Text.ElideRight; text: modelData.title; textFormat: Text.PlainText; color: window.chrome.foreground }
                Label { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.key; color: window.chrome.muted }
            }
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    readonly property bool globalKeyContext: dragSlot < 0 && !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen && !help.visible && !drawer.visible && !appMenu.visible && !paneMenu.visible
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
    Shortcut { sequence: "Escape"; enabled: window.overview && !shortcutSettings.visible && !fontSettings.visible && !commands.visible && !terminalAction.visible && !createDialog.visible && !window.appearanceOpen && !help.visible && !drawer.visible && !appMenu.visible; onActivated: { if (window.dragSlot >= 0) window.cancelDrag(); else window.toggleOverview() } }
    onActiveChanged: if (!active) cancelDrag()
    Component.onCompleted: { window.applyCatalog(studySession.initialCatalog, "initialize"); selected = remembered[0] ?? -1; Qt.callLater(function() { window.restoreView(studyView.initial); window.viewReady = true; window.scheduleViewSave(); if (!window.overview && selected >= 0) window.panel(selected).terminal.forceActiveFocus() }) }
}
