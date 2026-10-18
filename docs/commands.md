# Commands palette

Choose **Open Commands** in the hamburger menu, **Super+Alt+P** on Linux, or
**Command+Option+P** on macOS. The fresh keyboard model now supports configurable bindings,
navigation shortcuts and sending an intercepted key to the terminal. Defaults are
editable through Shortcut settings. See [shortcuts](shortcuts.md).

Type to filter action titles and descriptions. Up/Down or the wheel selects a
result; Enter or clicking a result runs it. Escape/Done closes the palette. Text
fields support desktop select-all and paste. A no-match Enter does nothing.
Unavailable actions remain visible with their reason, such as workspace capacity,
no following terminal, or a terminal that has not been confirmed as stopped.

The initial actions cover:

- Overview and application appearance.
- Workspace creation, rename, project-folder editing and confirmed stop.
- Terminal creation, rename, confirmed stop and confirmed replacement shell.
- Previous/next terminal and previous/next workspace, retaining existing shells.
- Shortcut settings, copy/paste and Send next key to terminal.

Actions apply to the active workspace or focused terminal. Opening a dialog from
an action consumes the dispatch key: Enter cannot also save or confirm the new
dialog. Stop/replacement dialogs require a mouse confirmation or an explicit
Tab/Shift+Tab move to their confirmation button followed by Enter/Space. Opening
Enter cannot stop or replace a process. The palette never takes another window's terminal ownership.
It opens from the working view, and existing dialogs, appearance and overview have
input precedence; there are no nested palettes. Ctrl-C/D/Z keep their usual terminal
meaning outside the palette.

The palette is client presentation state, with a fixed local action registry and
no new service messages or runtime protocol changes. Session actions call existing
management dialogs and methods. Catalog refresh pauses while choosing an action,
as for other dialogs, so the context does not change under the selection. The
palette's query and open state are temporary and are not restored at launch.
Shell output continues parsing while it is open. Results are bounded by the small
registry, with no filesystem reads, runtime queries or pane snapshots for matching.

## Validation

`just commands-gui-test` runs a disposable X11/Xvfb viewer with real shells and
checks mouse/chord entry, filtering, cancellation and no-match input isolation,
rename/create dialog handoff, unchanged shell PIDs during navigation, confirmation
safety, folder/theme/overview routing, capacity and stopped-state rejection,
default/custom navigation, binding recording/conflicts/cancel/clear/default,
saved bindings on reopen, modal scope, raw modified-arrow forwarding, unbound
clipboard shortcuts, minimum window geometry and ordinary Ctrl-C. Native macOS
shortcut input, palette
visuals and interaction remain pending. The Raylib input backend still does not
establish full IME, alternate-layout or native Wayland acceptance.

Terminal layout actions cycle the focused terminal between half, two thirds and
one third, toggle reversible full terminal-area width, or select an explicit
preset. They are client-only and work without session management or ownership.
Use Shortcut settings to change their bindings.

Tab/Shift+Tab moves between search, results and Done. Enter/Space on Done closes;
on results it runs the selected action. Arrow keys choose results, and Enter in
search retains quick dispatch. See [overlay focus](keyboard.md#dialog-and-overlay-focus).

**Find in terminal** opens a compact search bar for the focused pane's retained
output and scrollback. Enter moves to the next match; Shift+Enter moves back;
Escape restores the starting view. The action is configurable and listed in
shortcut help. See [terminal search](terminal-search.md) for literal matching,
Unicode/wrapping, live refresh and bounds.


## Qt candidate

`just toolkit-ui` now has a separate native-control palette with the same default
Command+Option+P / Super+Alt+P chord. It includes core workspace, layout, Overview,
clipboard and confirmed terminal-close actions. Supported managed application
bindings now load at startup, with active hints. The recorder/editor, UI override files, one-shot key forwarding
and terminal search still belong to the working viewer. See the
[Qt study](toolkit-ui-study.md#qt-command-palette) for exact scope and review steps.
