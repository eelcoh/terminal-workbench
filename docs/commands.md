# Commands palette

Open **Commands** at the bottom of the sidebar, **Ctrl+Shift+P** on Linux, or
**Command+Shift+P** on macOS. This is the first slice of the fresh keyboard model;
the opening chord is provisional, and per-action shortcut settings and forwarding
intercepted chords remain follow-up.

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

Actions apply to the active workspace or focused terminal. Opening a dialog from
an action consumes the dispatch key: Enter cannot also save or confirm the new
dialog. Stop/replacement actions retain the existing mouse confirmation; Enter
cannot stop a process. The palette never takes another window's terminal ownership.
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
minimum window geometry and ordinary Ctrl-C. Native macOS Command input, palette
visuals and interaction remain pending. The Raylib input backend still does not
establish full IME, alternate-layout or native Wayland acceptance.
