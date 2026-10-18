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
session catalog. Pane and Overview captions use cached runtime directory/Git/worktree metadata.

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
| Shift + Left / Right | Move the selected terminal one position left / right |
| Shift + Up / Down | Move to the adjacent workspace and follow its focus |
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

## Editing a workspace

Select a workspace, then open the application menu in the working view.
**Rename workspace…** edits its name; **Workspace folder…** edits the folder
used by new terminals. Each uses the same native fields and Save/Cancel pattern
as creation. Existing values open intact with the caret at the end. Use arrows,
Home/End, mouse selection or the platform clipboard shortcuts to edit a portion;
Select all explicitly replaces the value. Enter saves; Escape/Cancel discards.

The folder form explains that running shells keep their current directories.
An existing absolute path, `~/...` or a path relative to the study launcher's
home directory is accepted; spaces and UTF-8 use the existing service validation.
Empty values disable Save. Invalid folders remain editable with an inline error.
The form keeps its workspace identity while saving, disables repeated requests,
and closes only on success. Rename and folder changes are separate operations,
each using one existing service method (`workspace.rename` or
`workspace.directory`) without a new protocol or a partially applied combined Save.

Editing also works for an empty workspace and does not create a terminal.
Names update the drawer/top bar/Overview from the confirmed catalog. Directory
changes apply to subsequent new shells; existing shells retain their cwd, PID,
input connection, theme and grid. Cancelling changes none of those facts.
This still edits only the private disposable study catalog. Editing the saved
production catalog remains a later slice.

### Browsing for a folder

**Browse…** is next to the folder field in New workspace and Workspace folder.
It uses Qt Quick Dialogs' platform folder picker, with Qt's standard fallback
when a native dialog is unavailable. The starting location follows the field:
absolute, `~/...` and relative paths are resolved, and a missing or non-directory
path starts at its nearest existing parent. Resolution runs only when Browse
opens, outside pane layout and rendering. Each opening follows the current
field, rather than retaining navigation from an earlier picker.

Choosing a folder fills the field and returns focus to it. It does not create a
workspace or save a setting: use **Create/Save** afterward. Picker Cancel/Escape
keeps the form and typed path; cancelling the form also discards an accepted
picker choice. Paths cross the Qt boundary as file URLs and are converted with
Qt's local-file conversion, preserving spaces, Unicode, `#`, `%` and `&`.
The service still validates the path on Save, including deletion/access races.

The opt-in UI test report forces Qt's standard in-window fallback so automated
checks do not depend on the host desktop portal. Native xcb/Wayland keyboard and
pointer checks open Browse from creation and editing, cancel/reopen it, choose
folders, verify no implicit catalog writes, and start a real shell in a selected
Unicode folder. This validates the shared workflow, not a native portal or Mac
panel. Review the actual platform picker on Mac/Linux with `just toolkit-ui`;
The owner confirmed the native Mac picker works; physical Linux picker/portal
acceptance remains open.

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

Workspace theme choices are kept only for the current study window, like its
other presentation preferences. Imported palette files are saved separately as
described below. Dotfile defaults and saved workspace appearance integration are
later work; this does not change the daily-use viewer's selected theme.

## Importing TerminalColors and Ghostty themes

Open **Appearance → Import…**. Paste a local Ghostty color-file path, choose it
with Browse, or paste a [TerminalColors theme link](https://terminalcolors.com/ghostty/).
Both a theme page such as `/themes/nord/default/` and its direct Ghostty download
link are accepted. A name is inferred from the file/theme; an optional name and
author/license notes can be supplied. **Import & preview** validates and saves
the palette, then previews this workspace in Appearance. **Apply** keeps the
workspace's new look; Appearance **Cancel** restores the prior look. An imported
palette remains in the gallery for future use after Appearance cancellation.
Cancelling the import form or an in-flight download adds nothing.

Imports retain the exact original text, source URL/file reference and supplied
attribution. They live under the normal Workbench configuration directory at
`themes/imports/`: content-addressed `.ghostty` files plus an atomic `imports.json`
index. The study's workspace/session catalog remains private and disposable;
imported colors are a separate, explicit durable collection. Existing imports
load offline without requests to the website. The importer preserves supplied
comments/licenses; it does not infer redistribution rights or bundle downloaded
TerminalColors files with Workbench. The live export checked on 2026-10-18 had
Ghostty color fields and no embedded author/license notice.

Only color fields reach the terminal defaults. Unknown Ghostty settings are
counted and ignored; files, commands and keybindings are never followed or run.
Explicit RGB cells and application palette overrides survive preview/apply.
Invalid colors, empty/unsupported files, non-UTF-8/NUL text, files/downloads over
64 KiB and a collection beyond 64 imports fail without replacing existing themes.
Exact duplicate content selects the existing palette. Corrupt saved data stays
untouched and built-in palettes remain usable. Concurrent window imports merge
under a short lock; no lock spans a network request.

Downloads start only when Import is invoked. HTTPS requests are limited to the
TerminalColors Ghostty export endpoint, with a deadline, bounded buffering and
no automatic redirect or retry. Broken/unavailable links leave the form editable
and the active theme intact. The link adapter is based on the site's currently
published export URLs; a local downloaded Ghostty file also uses the same parser.
No website images, catalog scraping or background update requests are needed.

`just toolkit-theme-import-test` runs offline on Mac/Linux with temporary stores
and deterministic network replies. It covers duplicate/offline reload, source
metadata, stale-window merging, writer locks, checksums/corruption, bounds,
unsupported hosts, redirected/failed downloads, cancellation and deadlines.
Native xcb/Wayland UI tests exercise import preview/Apply/Cancel, a native-input
file chooser cancellation, invalid files, persistent metadata, deduplication and
unchanged original terminal PIDs/grids/input. A real TerminalColors Nord export
and its live link were also verified through Qt HTTPS/parsing in disposable storage;
external assets were not committed. Native Mac import review remains open.

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
Touchpad two-finger scrolling retains its Overview behavior.

Add **Shift** to the normal navigation chord to move the selected terminal:
Cmd–Option–Shift on Mac, Super–Alt–Shift on Linux. Left/Right reorder one position;
Up/Down append it to the adjacent workspace. There is no boundary wrapping.
The same terminal keeps focus after a confirmed move, while empty source workspaces
retain an empty focus state. In Overview the viewport follows the moved selection.
The pane layout menu also exposes these actions. Full destinations produce a short
capacity notice; their move shortcuts remain consumed and never reach the shell.
Move shortcuts are inactive in dialogs, menus, help, and during a pointer drag;
while a service request is pending they are consumed without duplicate mutations. In the working terminal, an ordinary click focuses
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
Editing checks cover in-place name/path changes, empty workspaces, cancelled
blank names, invalid folder errors and their correction. Real shells verify
that an edited folder applies to new terminals while the existing shell keeps
its cwd; original raw readers retain their PIDs, grids and isolated input.
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
confirmed that Appearance, cursor styling, workspace editing and native folder
browsing work. The latest keyboard moves, theme import and agent Overview need
native Mac review. Agent metadata is implemented as described below; production
session persistence and dotfile/font/shortcut integration remain later slices.
Lifecycle and ownership controls are described below. Typography, motion, accessibility and cache/backing-
surface budgets remain adoption gates. Review this interaction slice before
expanding it into the production frontend.


## Live context and agents

Pane headers show the current directory, or repository · branch · linked worktree
when the runtime knows it. Overview keeps these captions at a readable size above
scaled previews. Remote sessions show “Remote session” when local process context
would be misleading. Unknown context falls back to “Terminal”. Shell titles and
terminal output cannot set these facts.

The small Agents rail action opens an agent-only Overview. “Show: All terminals” /
“Show: Agents” and “State: All states” / “State: Needs input” state their actual
filters and highlight active restrictions. No matches shows an explicit message.
Filters update from real runtime state without replacing terminal items or
changing PTY geometry. A filtered drag maps visible neighbors back to the actual
catalog order, preserving hidden terminals.

Agent captions report Working, Needs input, Ready or Status unknown. Process
identification can recognize a plain agent invocation; reliable attention status
uses the existing `wb-*` launchers/hooks. Identification alone does not establish
that an agent is working or waiting. Only the runtime's attested lifecycle reports
set these states. Current agent/runtime support is unchanged.

The Qt adapter opens independent `CTX1` and `WAT2` read-only subscriptions to each
existing terminal control socket. It never attaches another PTY owner, starts a
runtime, runs Git, or polls process trees. Frames, strings and initial reply time
are bounded; identical updates do not notify the UI. Malformed/closed metadata
feeds clear only their own cached facts; terminal input and the other subscription
remain independent. Refresh reconnects failed feeds explicitly, with no retry timer.

`just toolkit-runtime-facts-test` runs the socket/parser contracts without a display.
The native UI suite also exercises real cwd/Git/linked-worktree and Codex lifecycle
reports with private test processes. Native Mac UI acceptance remains open.

To review manually, run `just toolkit-ui`, `cd` into a repository or linked
worktree, then launch an installed agent with its `wb-*` wrapper. Check the pane
caption and Agents Overview; use Needs input while the agent requests a decision,
then verify it leaves that filter after work resumes. Switch to All terminals and
All states to restore the complete overview.


## Qt chrome aligned with the working viewer

The Qt study now uses the spike's bundled Adwaita Sans for interface text,
with native toolkit text rendering and separate terminal typography. Original
geometric icons replace font-dependent symbols. The compact 40-pixel top bar
and 44-pixel rail restore the left menu placement, workspace label, centered
pane-position pill, focused/count text and adjacent New terminal action.

Chrome colors follow the same light/dark mixing rules as the working viewer,
including cursor-derived accents and semantic attention colors. Menus, section
separators, check marks, disabled actions, tooltips and workspace rows use shared
controls. Long names elide; eight indicators, count and actions fit the minimum
680-pixel window. Agent attention has a rail count and pane-position dot, with
status text retained separately in the pane header. Idle previews use an arrow;
the grab cursor appears during dragging only.

This ports the approved visual direction into `just toolkit-ui`; `just spike`
remains the daily-use frontend. Native Mac visual acceptance is still required.
The UI suite checks actual bundled font selection, minimum-width non-overlap,
long workspace names, attention transitions, existing keyboard/menu/focus flows,
and stable terminal identities/geometry through overlays. Review captures include
`chrome-menu.png`, `chrome-workspaces.png` and `chrome-narrow.png` under
`.local/artifacts/qt-ui/{xcb,wayland}/`.


## Qt terminal lifecycle and ownership

The Qt study now shows a compact status card when a terminal is disconnected.
Recorded exit codes/signals, explicit stops and missing-exit evidence come from
the existing session service. Unknown evidence stays unknown. Status checks are
bounded, coalesced per terminal and request driven; they never start a missing
service, replace a shell or attach a PTY owner. Check again refreshes those facts.

- **Reconnect** attempts a normal attachment to the running process. If another
  window owns it, that window keeps control.
- **Move here…** asks for confirmation before transferring the existing process.
  The old owner receives an explicit release state and can later reconnect.
- **Start new shell** is offered only for a stopped runtime. The existing service
  revalidates that no shell is live and starts in the workspace's configured
  folder. It preserves terminal identity, theme and width; it starts a new process.
- **Close terminal…**, in the pane menu, asks before stopping the process and
  removing its catalog entry. Cancel receives initial focus; Enter cannot silently
  confirm destruction. Other terminals retain their live items, PIDs and input.

Snapshot reconnect commits a newly decoded client replica and rebuilds its render
handles without taking terminal authority from the runtime. A failed snapshot
keeps the old display; failures stay local to that terminal. Removed panes retire
their context/agent/status observers. Selection, widths and remembered workspace
focus are reconciled by stable identity rather than retaining shifted indices.

This slice also fixes returning from Overview with an obsolete animated scroll
position. Reveal uses the intended scroll target and clamps to the current
workspace; a disconnected selected terminal's controls remain reachable.

`just toolkit-lifecycle-test` exercises lifecycle-only requests, fragmented,
malformed, oversized, closed and timed-out replies, missing-service non-start and
observer retirement without a display. The native UI suite uses real runtimes to
exercise exit 42, missing evidence, new-shell recovery, explicit ownership transfer,
non-stealing reconnect, service downtime and confirmed/cancelled close. It verifies
stable sibling UI instances and reader PIDs, then the normal narrow-window layout.

For manual Mac/Linux review, run `just toolkit-ui`, enter `exit 42`, inspect the
reason and choose Start new shell. Open that pane's overflow menu, choose Close
terminal…, cancel once, then confirm. The workspace should become empty when its
last terminal closes; its neighbours should keep running. Native Mac acceptance
of this slice remains pending. Paired-window saved-session integration remains a
later slice; the study launcher still owns only private disposable sessions.
