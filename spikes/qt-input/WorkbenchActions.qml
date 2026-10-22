import QtQuick

// Client-only registry. Matching and availability never query the runtime.
QtObject {
    required property var host
    required property var session
    function row(id, title, detail, available, reason) {
        return {id: id, title: title, detail: detail, key: studyShortcuts.labels[id] || "", available: available, reason: available ? "" : reason}
    }
    readonly property bool hasPane: host.selected >= 0 && host.terminalItems.length > host.selected && host.panel(host.selected) !== null
    readonly property bool exitedPane: hasPane && host.exitedTerminal(host.panel(host.selected).terminalId)
    readonly property bool hasWorkspace: host.spaces.length > 0
    readonly property bool workingPane: hasPane && !host.overview
    readonly property var neighbours: host.overview ? host.overviewIndices(host.workspace) : host.indices(host.workspace)
    readonly property int position: neighbours.indexOf(host.selected)
    readonly property var entries: [
        row("commands", "Open Commands", "Search application actions", true, ""),
        row("shortcut-settings", "Shortcut settings…", "Record and save application bindings", true, ""),
        row("font-settings", "Terminal text…", "Choose a bundled font and preview its size", true, ""),
        row("font-increase", "Increase terminal text size", "Larger text across this window", studyFonts.current.size < 32, "Already at 32 pt"),
        row("font-decrease", "Decrease terminal text size", "Smaller text across this window", studyFonts.current.size > 10, "Already at 10 pt"),
        row("font-reset", "Reset terminal text size", "Use dotfile size and retain the chosen font", true, ""),
        row("new-terminal", "New terminal", "Start a shell in this workspace’s folder", host.canCreateTerminal, session.busy ? "Another change is in progress" : "Workspace already has eight terminals"),
        row("new-workspace", "New workspace…", "Choose a name and project folder", host.canCreateWorkspace, session.busy ? "Another change is in progress" : "Already at eight workspaces"),
        row("overview", host.overview ? "Leave Overview" : "Show Overview", "See workspaces and live terminal previews", true, ""),
        row("agent-info", "Agent details…", "Explain detected identity, lifecycle state and optional setup", hasPane, "No terminal selected"),
        row("agents", "Show agents", "Open the agent list across workspaces", true, ""),
        row("previous-terminal", "Previous terminal", "Focus the terminal to the left", position > 0, "No previous terminal in this view"),
        row("next-terminal", "Next terminal", "Focus the terminal to the right", position >= 0 && position + 1 < neighbours.length, "No next terminal in this view"),
        row("previous-workspace", "Previous workspace", "Switch to the workspace above", host.workspace > 0, "Already at the first workspace"),
        row("next-workspace", "Next workspace", "Switch to the workspace below", host.workspace + 1 < host.spaces.length, "Already at the last workspace"),
        row("rename-workspace", "Rename workspace…", "Edit the current workspace name", !host.overview && hasWorkspace && !session.busy, !hasWorkspace ? "No workspace selected" : host.overview ? "Leave Overview first" : "Another change is in progress"),
        row("workspace-folder", "Workspace folder…", "Choose the folder used by new shells", !host.overview && hasWorkspace && !session.busy, !hasWorkspace ? "No workspace selected" : host.overview ? "Leave Overview first" : "Another change is in progress"),
        row("appearance", "Workspace appearance…", "Choose a Ghostty / TerminalColors theme", !host.overview && hasWorkspace, !hasWorkspace ? "No workspace selected" : "Leave Overview first"),
        row("cycle-width", "Cycle terminal width", "Half → two thirds → one third", workingPane, host.overview ? "Leave Overview first" : "No terminal selected"),
        row("full-width", "Toggle full width", "Fill the terminal area or restore its width", workingPane, host.overview ? "Leave Overview first" : "No terminal selected"),
        row("move-left", "Move terminal left", "Reorder within this workspace", host.canReorder(-1), session.busy ? "Another change is in progress" : "No previous position"),
        row("move-right", "Move terminal right", "Reorder within this workspace", host.canReorder(1), session.busy ? "Another change is in progress" : "No next position"),
        row("move-up", "Move to workspace above", "Keep the same running process", host.canMoveWorkspace(-1), "No available workspace above"),
        row("move-down", "Move to workspace below", "Keep the same running process", host.canMoveWorkspace(1), "No available workspace below"),
        row("session-stop", "Stop all terminals…", "Confirm before stopping shells across this session’s workspaces and windows; keep saved organization", host.terminalItems.length > 0 && !session.busy, session.busy ? "Another change is in progress" : "No saved terminals"),
        row("close-workspace", "Remove workspace…", "Confirm before stopping its terminals and removing this workspace", hasWorkspace && !session.busy, !hasWorkspace ? "No workspace selected" : "Another change is in progress"),
        row("close-terminal", exitedPane ? "Close pane" : "Close terminal…", exitedPane ? "Remove this exited pane" : "Confirm before stopping and removing this terminal", hasPane && !session.busy, session.busy ? "Another change is in progress" : "No terminal selected"),
        row("forward", "Send next key to terminal", "Bypass application shortcuts once; Escape cancels", workingPane && host.panel(host.selected) !== null && host.panel(host.selected).terminal.ready, "No connected terminal selected in the working view"),
        row("terminal-search", host.overview ? "Find in Overview" : "Find in terminal", host.overview ? "Find workspace, folder, Git or agent metadata" : "Search literal text in this terminal and its scrollback", host.overview || (workingPane && host.panel(host.selected) !== null && host.panel(host.selected).terminal.searchable), "No retained terminal selected in the working view"),
        row("copy", "Copy selection", "Copy selected terminal text to the clipboard", workingPane, "No terminal selected in the working view"),
        row("paste", "Paste", "Paste clipboard text into the focused terminal", workingPane && host.panel(host.selected) !== null && host.panel(host.selected).terminal.ready, host.overview ? "Leave Overview first" : "No connected terminal selected"),
        row("motion", host.reducedMotion ? "Enable animations" : "Reduce motion", "Change interface transitions for this window", true, ""),
        row("help", "Keyboard shortcuts", "Show the navigation and layout bindings", true, ""),
        row("configuration", "Configuration", "Show managed files and preference locations", true, ""),
        row("configuration-reload", "Reload configuration", "Read font and shortcut files without restarting terminals", !studyConfiguration.editing, "Close settings editors first"),
        row("about", "About Terminal Workbench", "Show this client build and copy installation diagnostics", true, "")
    ]
    function find(id) { return entries.find(entry => entry.id === id) }
}
