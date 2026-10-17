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

## Workspace Appearance

From the working view, open **application menu → Appearance…**. Search the
existing offline Ghostty-compatible catalog or filter All/Dark/Light. Select a
card to preview every terminal in this workspace and its chrome together.
Arrow Down from search enters the results; Return applies the selected preview.
**Apply** keeps the choice for this workspace in this study window. **Cancel**
or Escape restores the previous terminal and chrome appearance. Other workspaces
keep their themes, and new terminals inherit their workspace's selection. Empty
workspaces can also receive a theme before their first terminal is created.

The picker keeps a steady dark surface through light/dark previews, labels the
target workspace and current preview, and shows an explicit no-results state.
Appearance has no extra Overview button. The seven palettes are Workbench
Night/Dawn, Dracula, and the four bundled Catppuccin variants. The C bridge reuses
the existing bounded color-file parser and generated catalog; source revisions
and MIT notices remain in [themes/README.md](../themes/README.md).

Theme selection is client presentation state. It updates default colors on the
existing Ghostty presentation replica, plus cursor/selection colors. It neither
sends a theme escape sequence to a PTY nor mutates the session catalog. Explicit
RGB cells and program OSC palette overrides remain intact. Preview/apply changes
colors without resizing grids, replacing processes or reconnecting terminals.
Parsing/catalog construction happens once, outside refresh/paint loops; theme
application happens when a selection changes. Hidden terminals continue parsing
and skip raster work until revealed.

The study's themes are kept only for its current window lifetime, like its other
presentation preferences. Dotfile defaults, theme import and saved appearance
integration are later work; this does not change the daily-use viewer's settings.

## Moving terminals in Overview

Open Overview and drag a terminal preview within its workspace to reorder it,
or onto another workspace to move it. A thin marker shows the insertion point;
the destination outline and floating label identify the workspace and position.
Empty workspaces accept drops too. Hold near the top/bottom edge to scroll to
workspaces outside the viewport. Escape cancels the drag while keeping Overview
open; dropping outside the viewport also cancels. A full destination is marked
unavailable. Reordering within a full workspace remains possible.

A drop submits one asynchronous `terminal.move` through the existing service.
Previews stay in their current places until that request succeeds. Cancellation
and rejected drops leave the catalog untouched; uncertain replies use the
existing notice/Refresh path without automatically resending the mutation.
The same terminal objects remain attached, preserving shell PIDs, actual cwd,
input ownership and width preferences. A moved terminal adopts its destination
workspace's theme. Focus memory is reconciled by terminal identity, and the
working strip, indicators and keyboard navigation follow the confirmed order.

Dragging currently starts from Overview with a mouse/trackpad press and drag.
Touchpad two-finger scrolling retains its Overview behavior. Keyboard-only move
commands remain later work. In the working terminal, an ordinary click focuses
and clears text selection; text selection starts only after the platform drag
threshold, so a click no longer leaves a selected cell behind.

## Cursor focus and application shapes

The focused terminal in an active window uses a filled block by default;
inactive panes, modal-control focus and inactive windows use an outline.
Application-requested bar/underline shapes are respected while active, as are
cursor visibility and an explicitly hollow style. Wide graphemes receive a
whole-grapheme caret. The cursor uses the effective theme/program cursor color.
A filled block redraws the shaped character in the cell's background color so
moving onto text does not obscure it.

Focus changes repaint the caret region using retained state. The small glyph
mask is cached on the GUI thread and reused during paint; focus changes do not
rescan the VT or rerasterize terminal rows. There is no new blinking timer or
idle animation; blink timing remains follow-up work.

Native xcb/Wayland checks inspect captured pixels for filled/outline cursors,
readable glyphs under blocks, and hidden cursors; they also drive app-requested
bar/underline shapes and pane/modal focus transfers without changing readers or
grids. Artifacts include `cursor-*.png` and opt-in `report.json.cursor.json`
pixel counts. On Mac, check pane changes, opening a menu/dialog, switching to
another app and returning, plus application-requested cursor shapes.

## Evidence and next scope

`just toolkit-ui-test` drives real native keyboard/pointer input against six
raw PTY readers on private xcb and native Wayland (DISPLAY unset for Qt).
It checks native horizontal wheel scrolling without focus/grid changes, exact
routing/no leaked application shortcuts, independent width
changes and full-width restoration, workspace focus memory, help/Escape,
empty workspace, Overview preview activation, stable grids through Overview
and unchanged reader PIDs. Native drag checks cover reorder/navigation, cancellation,
empty-workspace moves, rejection of full destinations, same-workspace reorder at
capacity, destination themes, automatic edge scrolling and unchanged
original-reader identities, input and grids. A plain click leaves no selection.
Creation checks add terminals to existing/empty
workspaces, reject invalid folders without catalog changes, cancel/retry the
form, verify actual new-shell cwd and preserve the six original raw readers.
Theme checks drive the real menu/search/results UI: light preview and Cancel,
Dracula Apply, no-results search, workspace/chrome isolation and new-terminal
inheritance. They inspect actual rendered-cell colors for indexed ANSI, explicit
RGB and a program OSC palette override, preserve geometry/PIDs and verify that
appearance leaves the runtime catalog unchanged. Captures include the light
preview and empty search result.
A launcher check creates additional shells after startup and verifies complete
shell/service cleanup on both normal and failed viewer exit. Offscreen columns and background workspaces consume
new output without rasterizing rows, then show current content on reveal.
Artifacts are `.local/artifacts/qt-ui/{xcb,wayland}/`, including working and
Overview captures and opt-in disposable-terminal reports. Reports are never
created by ordinary interactive runs.

The owner confirmed the initial UI and creation flows on Mac and Linux, and
confirmed that Appearance and cursor styling work. The new click/drag interactions
still need native Mac review. This slice does not yet
implement existing workspace rename/folder editing, a native folder picker, agents,
persistence, configured shortcuts or session ownership workflows.
It avoids dead controls for those actions. They remain later design slices,
along with typography, motion, accessibility and cache/backing-surface budgets.
Review this interaction slice before expanding it into the production frontend.
