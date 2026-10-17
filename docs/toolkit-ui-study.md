# Qt live UI study

Run `just toolkit-ui` on Linux or Mac. It opens six disposable shells split
between two named workspaces, plus an empty workspace. Closing the window
stops every shell created in the study, including new ones. It does not open the saved Workbench catalog. The
existing `just spike` application remains the daily-use viewer; Qt adoption
has not been decided.

The slice implements the approved minimal rail/top bar, workspace drawer,
focus/visible/offscreen pane indicators and small pane corner radius. Overview
shows live scaled terminal surfaces grouped by workspace. Click a preview to
return to that terminal, or use Done/Escape. Its geometry is a visual transform:
terminal grids and processes do not change merely because Overview opened.
The initial workspace names are fixtures; new names and folders use the private
session catalog. Pane labels are still study labels, not live cwd/Git metadata.

Width changes affect the selected terminal only; full width restores its prior
preset. Terminal geometry changes once when the width preference changes,
while positions animate independently. Horizontal wheel/trackpad deltas pan the
strip; vertical terminal input retains terminal scrollback. Overview can scroll
vertically. The owner confirmed the initial interaction slice works on Mac and Linux.
Further physical trackpad and visual quality review remain part of the UX gate.
The application menu offers Reduced motion and separated command groups.

## Keyboard review

Use **Cmd–Option** on Mac or **Super–Alt** on Linux:

| Key | Action |
| --- | --- |
| Left / Right | Previous / next terminal in this workspace |
| Up / Down | Previous / next workspace, restoring its focused terminal |
| T | New terminal in the current workspace |
| N | New workspace (name and folder) |
| O | Enter / leave Overview |
| R | Cycle selected width: half → two thirds → one third |
| F | Toggle full width / restore |
| / | Shortcut help |

Escape closes Overview/overlays and otherwise reaches the shell. Copy/paste
retain the platform bindings proved by the Qt terminal probe. The top bar
indicators, workspace drawer, pane layout menu and help button provide mouse
access. Bindings are fixed for this isolated study; dotfile shortcut settings
and the production command registry are not connected yet.

## Creating workspaces and terminals

Use **+** in the top bar or the New terminal shortcut/menu entry. New workspace
is in the workspace drawer and application menu, with its shortcut above. Enter
a name and existing folder path; relative paths resolve from the launcher's
home directory and `~` is supported. Create opens the new workspace with its
first shell. Cancel/Escape keeps the existing selection and catalog.

Name and folder are native editable text fields, with cursor selection and
in-place edits. Enter moves from name to folder, then submits. Empty fields
disable Create; invalid paths and service validation failures stay in the form.
Creation is asynchronous; repeated requests are disabled while pending. The
existing service enforces eight workspaces/eight terminals per workspace, and
creation controls disable at those limits. New terminals inherit their workspace
folder, width half and their own stable identity. Existing terminals remain
attached; widths, focus memory and processes are preserved. Overview wraps more
than three terminals onto additional rows within each workspace.

The Qt adapter uses the existing JSON service methods `terminal.create` and
`workspace.create_at` against a private service started by the launcher. It does
not own shells, write the catalog, or introduce another mutation protocol. A
failed or lost response produces a notice; sent mutations are never retried
automatically. Refresh reads the catalog without replacing existing live items
or restarting stopped shells. Closing stops all study runtimes and its service.
This remains disposable; saved user sessions are not accessed or migrated.

## Evidence and next scope

`just toolkit-ui-test` drives real native keyboard/pointer input against six
raw PTY readers on private xcb and native Wayland (DISPLAY unset for Qt).
It checks native horizontal wheel scrolling without focus/grid changes, exact
routing/no leaked application shortcuts, independent width
changes and full-width restoration, workspace focus memory, help/Escape,
empty workspace, Overview preview activation, stable grids through Overview
and unchanged reader PIDs. Creation checks add terminals to existing/empty
workspaces, reject invalid folders without catalog changes, cancel/retry the
form, verify actual new-shell cwd and preserve the six original raw readers.
A launcher check creates additional shells after startup and verifies complete
shell/service cleanup on both normal and failed viewer exit. Offscreen columns and background workspaces consume
new output without rasterizing rows, then show current content on reveal.
Artifacts are `.local/artifacts/qt-ui/{xcb,wayland}/`, including working and
Overview captures and opt-in disposable-terminal reports. Reports are never
created by ordinary interactive runs.

The owner confirmed the initial UI slice on Mac and Linux. The new creation
flow still needs native Mac review. This slice does not yet implement existing
workspace rename/folder editing, a native folder picker, agents, theme selection,
drag/reorder, persistence, configured shortcuts or session ownership workflows.
It avoids dead controls for those actions. They remain later design slices,
along with typography, motion, accessibility and cache/backing-surface budgets.
Review this interaction slice before expanding it into the production frontend.
