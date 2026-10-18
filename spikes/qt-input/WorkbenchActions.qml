import QtQuick

// Client-only registry. Matching and availability never query the runtime.
QtObject {
    required property var host
    required property var session
    function row(id, title, detail, available, reason) {
        return {id: id, title: title, detail: detail, key: studyShortcuts.labels[id] || "", available: available, reason: available ? "" : reason}
    }
    readonly property bool hasPane: host.selected >= 0 && host.terminalItems.length > host.selected && host.panel(host.selected) !== null
    readonly property bool workingPane: hasPane && !host.overview
    readonly property var neighbours: host.overview ? host.overviewIndices(host.workspace) : host.indices(host.workspace)
    readonly property int position: neighbours.indexOf(host.selected)
    readonly property var entries: [
        row("commands", "Open Commands", "Search application actions", true, ""),
        row("font-settings", "Terminal text…", "Choose a bundled font and preview its size", true, ""),
        row("font-increase", "Increase terminal text size", "Larger text across this window", studyFonts.current.size < 32, "Already at 32 pt"),
        row("font-decrease", "Decrease terminal text size", "Smaller text across this window", studyFonts.current.size > 10, "Already at 10 pt"),
        row("font-reset", "Reset terminal text size", "Use dotfile size and retain the chosen font", true, ""),
        row("new-terminal", "New terminal", "Start a shell in this workspace’s folder", host.canCreateTerminal, session.busy ? "Another change is in progress" : "Workspace already has eight terminals"),
        row("new-workspace", "New workspace…", "Choose a name and project folder", host.canCreateWorkspace, session.busy ? "Another change is in progress" : "Already at eight workspaces"),
        row("overview", host.overview ? "Leave Overview" : "Show Overview", "See workspaces and live terminal previews", true, ""),
        row("agents", "Show agents", "Open Overview filtered to coding agents", true, ""),
        row("previous-terminal", "Previous terminal", "Focus the terminal to the left", position > 0, "No previous terminal in this view"),
        row("next-terminal", "Next terminal", "Focus the terminal to the right", position >= 0 && position + 1 < neighbours.length, "No next terminal in this view"),
        row("previous-workspace", "Previous workspace", "Switch to the workspace above", host.workspace > 0, "Already at the first workspace"),
        row("next-workspace", "Next workspace", "Switch to the workspace below", host.workspace + 1 < host.spaces.length, "Already at the last workspace"),
        row("rename-workspace", "Rename workspace…", "Edit the current workspace name", !host.overview && !session.busy, host.overview ? "Leave Overview first" : "Another change is in progress"),
        row("workspace-folder", "Workspace folder…", "Choose the folder used by new shells", !host.overview && !session.busy, host.overview ? "Leave Overview first" : "Another change is in progress"),
        row("appearance", "Workspace appearance…", "Choose or import a Ghostty / TerminalColors theme", !host.overview, "Leave Overview first"),
        row("cycle-width", "Cycle terminal width", "Half → two thirds → one third", workingPane, host.overview ? "Leave Overview first" : "No terminal selected"),
        row("full-width", "Toggle full width", "Fill the terminal area or restore its width", workingPane, host.overview ? "Leave Overview first" : "No terminal selected"),
        row("move-left", "Move terminal left", "Reorder within this workspace", host.canReorder(-1), session.busy ? "Another change is in progress" : "No previous position"),
        row("move-right", "Move terminal right", "Reorder within this workspace", host.canReorder(1), session.busy ? "Another change is in progress" : "No next position"),
        row("move-up", "Move to workspace above", "Keep the same running process", host.canMoveWorkspace(-1), "No available workspace above"),
        row("move-down", "Move to workspace below", "Keep the same running process", host.canMoveWorkspace(1), "No available workspace below"),
        row("close-terminal", "Close terminal…", "Confirm before stopping and removing this terminal", hasPane && !session.busy, session.busy ? "Another change is in progress" : "No terminal selected"),
        row("copy", "Copy selection", "Copy selected terminal text to the clipboard", workingPane, "No terminal selected in the working view"),
        row("paste", "Paste", "Paste clipboard text into the focused terminal", workingPane && host.panel(host.selected) !== null && host.panel(host.selected).terminal.ready, host.overview ? "Leave Overview first" : "No connected terminal selected"),
        row("motion", host.reducedMotion ? "Enable animations" : "Reduce motion", "Change interface transitions for this study", true, ""),
        row("help", "Keyboard shortcuts", "Show the navigation and layout bindings", true, "")
    ]
    function find(id) { return entries.find(entry => entry.id === id) }
}
