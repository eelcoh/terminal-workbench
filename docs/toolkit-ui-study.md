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

Vertical terminal scrolling retains fractional pixel and wheel-angle movement
instead of discarding movements smaller than a text row. Sideways jitter during
a vertical gesture does not pan the strip or block terminal scrolling. X11 uses
angle deltas because Qt documents its pixel deltas as unreliable; Mac/Wayland
use pixel deltas when supplied. Delivered signs follow the system scroll policy
without applying `inverted` a second time.

`just toolkit-scroll-test` checks accumulation, direction, phases and bounds.
`just toolkit-ui-test` also sends wheel events through the actual window/QML
path, checking history/live movement, diagonal isolation and unchanged PTY
input/readers/grids on X11 and Wayland. Physical Mac trackpad and Linux device
direction review are still required. To capture deltas without terminal text,
start with `WORKBENCH_SCROLL_DEBUG=1 just toolkit-workbench` and retain the
launcher output/log. Qt's [wheel event documentation](https://doc.qt.io/qt-6/qwheelevent.html)
describes the platform delta conventions.

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
access. Managed application bindings now load at startup; the recorder/editor,
generated UI overrides and production-session integration remain follow-ups.

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

## TerminalColors and Ghostty themes

Use the [drop-in theme directory](theme-directory.md). Appearance’s help icon explains that
folder contents appear automatically and offers an **Open themes folder** link
to the file manager. Add files there, then reopen Appearance to refresh the list.
Select a theme in Appearance and choose **Apply**; do not open the theme file.

The earlier import form is no longer exposed. Previously imported palettes still
load offline from `themes/imports/`. The import backend and its offline validation
tests remain available for existing stored data.

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
- **Close pane**, on an exited/stopped pane, removes it after a stopped-only service check, without another confirmation.
  It removes the pane without starting a replacement shell.
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


## Qt command palette

In `just toolkit-ui`, open **Open Commands** from the hamburger menu, or use
**Command+Option+P** on macOS / **Super+Alt+P** on Linux. Type to search titles
and descriptions; title matches rank first. Up/Down selects a result, Enter
runs it, and Escape or Done closes the palette. Tab/Shift+Tab cycles search,
results and Done. Clicking an available result also runs it. Unavailable actions
remain visible with a reason, and no-match Enter does nothing.

The palette covers creation, workspace name/folder, appearance/import access,
Overview/agents, focus/navigation, widths, terminal moves/confirmed close,
clipboard, motion and shortcut help. Actions target the current workspace and
focused terminal, with stable IDs checked again at dispatch. Existing service
commands and confirmation dialogs remain authoritative. Opening Enter does not
also submit the next dialog or confirm a close. Palette input never goes to a
terminal. Escape closes only the palette when opened over Overview.

The registry is client presentation state. Filtering performs no filesystem or
runtime requests, snapshots or process inspection. The palette never changes
terminal ownership. Its query/open state and motion toggle remain temporary in
this disposable study. Supported managed bindings now load at startup; the
recorder/editor, UI overrides, key forwarding and saved settings are follow-ups; these existing `just spike` features have not all moved to Qt.

`just toolkit-ui-test` exercises menu/chord entry, title-priority filtering,
no-match and disabled actions, arrow/Tab navigation, mouse activation, safe
creation/close handoff, Overview scope and minimum-width layout on private X11
and Wayland desktops. It also checks unchanged terminal objects, grids and raw
reader input across the palette flows. Review captures are `commands-search.png`
and `commands-narrow.png` under `.local/artifacts/qt-ui/{xcb,wayland}/`.
Native Mac keyboard/visual acceptance remains pending. `just spike` is still the
daily-use viewer; this does not adopt Qt as the production frontend.


## Qt font controls

Terminal text settings now preview/apply/cancel bundled JetBrains Mono or Fira
Code at 10–32 logical points across the study window. Commands and the default
application modifier with = / − / 0 provide size changes. New panes inherit the
window choice; existing PTYs retain their identities/processes. Dotfile font
baselines use the existing shared/platform parser at startup, with untouched
managed files and a visible fallback diagnostic. Changes remain window-local
and temporary; saved overrides and the other portable settings are follow-ups.
See [font controls and validation](terminal-fonts.md#qt-candidate-terminal-text-and-dotfile-defaults).


## Managed shortcut loading

The Qt candidate imports supported application chords from the existing managed
shared/platform configuration. Commands, menus, tooltip and help show the active
bindings. Unbinding releases the old chord to terminal input; modals own their
keys. Native copy/paste remains independent, with conflicting application chords
rejected. Qt-only move defaults yield to configured actions. No file is written,
no runtime protocol changes, and unsupported actions remain unintercepted.
The editor/recorder, generated UI override files and one-shot key forwarding are
follow-ups. See [configuration and validation](shortcuts.md#qt-candidate-managed-application-shortcuts).


## Drop-in theme directory

The primary Qt theme workflow now uses the per-user configuration `themes/`
directory. Appearance’s help icon explains automatic discovery and offers an **Open themes folder** link. Drop Ghostty files there,
including TerminalColors `.default` downloads, and reopen Appearance to select
them. Files remain read-only and offline; discovery does not require the optional
import form. Preview/Apply/Cancel preserves PTYs and terminal geometry. Invalid
files are skipped; editing/removing a file retains existing window colors until a
new theme is chosen. See [paths, usage and validation](theme-directory.md).

## Appearance preview clarity

Folder instructions and the folder link now live behind the small **?** help
icon beside the theme filters. Theme cards show sample terminal text on the
actual background, plus larger normal and bright ANSI swatches. Appearance keeps
input modal without dimming the workspace, so live preview colors match Apply.
Help closes independently with Escape; Apply and Cancel retain their behavior.

## Shortcut recording and saved UI bindings

Shortcut settings is available from the menu, Commands and shortcut help. It
records real chords with shell control/clipboard reservation and duplicate
checks. Drafts stay local until Apply; Cancel discards them. Clear and Use dotfile
provide unbinding and inherited-default restoration. Apply updates bindings/hints
and writes a sparse platform `ui/shortcuts.conf` layer in the existing format,
without modifying managed settings. UI overrides survive study-window closure;
workspace organization and font edits remain disposable. Changed external files
block saving until the app reopens. See [editor details](shortcuts.md#qt-shortcut-editor).

## Sending an intercepted chord to a terminal

Commands and the application menu offer **Send next key to terminal** for a
connected working-view terminal. The optional `forward` binding is managed by
the shortcut editor too. A compact banner shows the armed state and Cancel;
Escape and a ten-second timeout also cancel. Modifier presses do not consume it.
Focus, visibility, connection and composition changes retire an unused request.
The same live item encodes the next chord, bypassing application/clipboard and
host word-motion interception. Normal key handling resumes afterward. No runtime
or protocol changes are involved. See [Qt forwarding](shortcuts.md#qt-one-shot-forwarding).

## Durable terminal font defaults

Terminal text Apply now saves a sparse platform UI layer in
`ui/<platform>/terminal-font.json`, while managed fonts remain read-only. New
launches inherit saved face/size; existing other windows retain their choices.
Preview and Cancel do not save, and size shortcuts remain temporary until Apply.
Use dotfile defaults followed by Apply removes overrides. Invalid stores are
preserved; stale or failed saves keep the dialog open for Cancel. Font saving is
independent of the disposable session catalog. Saved-mode workspace themes and
per-window layouts now persist in separate view profiles. See [font persistence](terminal-fonts.md#qt-saved-font-defaults).

## Opt-in saved-session mode

Run `just toolkit-workbench` for the same Qt client on saved Workbench workspaces.
It shares the existing service and `.local/sessions` catalog, honoring
`WORKBENCH_STATE_DIR`. Closing detaches rather than stopping shells; normal
attachment never steals another window’s terminal. The private disposable
`just toolkit-ui` workflow is unchanged. View/theme restoration and automatic
peer-window catalog updates are implemented; **Refresh workspaces** remains an
explicit recovery action. See [saved-session review](qt-saved-sessions.md).

## View profiles for saved Qt windows

`just toolkit-workbench` now keeps each window’s layout, focus, Overview/filter
state and committed workspace themes in a separate leased Qt view profile.
Reopening reuses a free profile without restarting shells or taking ownership.
The disposable launcher still omits durable view paths. Theme snapshots keep
selected colors if their source file disappears; only Apply changes committed
colors. View saving is debounced and errors stay local without overwriting invalid
files. See [saved views](qt-saved-sessions.md#saved-qt-window-views) for paths,
limits and native review steps. The owner confirmed appearance retention with
`f80f00b`. Saved mode follows peer catalog changes automatically through filesystem
notifications and read-only service requests; disposable studies keep this feed
disabled. See [automatic updates](qt-saved-sessions.md#automatic-workspace-updates).

## Pinned navigation and a dedicated agent list

Open Workspaces from the rail and use the pin icon to keep the panel beside the
terminals. It reserves space rather than covering output. Workspace selection
keeps the pinned panel open and restores terminal focus; normal navigation
shortcuts still work. Unpinning returns to a temporary drawer, dismissible with
Escape. Below 920 pixels, the pinned panel falls back to the drawer; widening
restores it. Pinning is unavailable in an already narrow unpinned window.

The Agents rail entry and Show agents command open the panel’s separate Agents
tab. Each row shows agent identity, Working/Ready/Needs input/Unknown state,
workspace and terminal context. Select a row to focus its terminal, including in
another workspace. This does not take ownership from another window. An empty
list says no agents are detected; it does not reuse the spatial Overview or its
filters. The Overview identity/state filters remain available independently.

`just toolkit-workbench` saves the pin preference with each leased view profile;
`just toolkit-ui` keeps it only for the disposable window. Older saved profiles
open compact by default. Check both tabs, workspace selection, pin/unpin, a narrow
window, Overview and a live agent. Native Mac review remains pending.

## Overview refinement

Overview now gives each workspace a clear heading, folder context and terminal
count, with quiet separators between groups. Empty workspaces retain the plain
background and a small empty label. Terminal context and agent state sit below
each live preview at normal interface text sizes. Selection/hover outlines remain
unscaled, with room inside the clip for the first column’s border. Captions also
focus their terminal when clicked.

The grid uses up to three columns, reducing its column count as the terminal area
narrows. One/two-column layouts allow taller previews. Source terminal dimensions
and live readers remain unchanged when toggling Overview. The same column geometry
drives previews, captions, keyboard reveal and drag targets. Entering Overview
reveals the selection; workspace/terminal shortcuts keep the target visible.

Filters retain explicit identity/state labels, a match count and a Clear filters
action. An empty filtered view can return to all terminals without guessing which
filter is active. Dedicated agent navigation remains in the left panel.

Native Linux checks cover dense/empty workspaces, filtered drag, narrow two-column
layout, visible keyboard selection/captions, reset from no matches, unchanged PTY
grids and retained readers. Native Mac visual/trackpad acceptance remains pending.

## Workspace removal and the separate create action

The workspace list contains existing workspaces only. **New workspace…** lives
in a fixed footer below a divider, with a plus icon and the configured shortcut
hint. It stays available when the list scrolls. Up/Down/Home/End move focus among
existing workspace entries; Enter selects that workspace. Tab can reach separate
actions explicitly. The creation form opens only when its action is activated.

Each workspace row has its own quiet × button. It opens **Remove workspace?**
without selecting the row. The confirmation names the workspace and shows its
terminal count, explaining that running processes there will stop. Cancel receives
initial focus; Enter initially cancels. Escape also cancels. Nothing stops or
moves until **Remove workspace** is activated. The application menu and Commands
provide the same confirmed action for the selected workspace.

The target is a stable workspace ID. Peer renames/count changes update the
confirmation context; deletion elsewhere dismisses it. Existing service guards
and close journaling still apply. Failed requests keep the confirmation and show
the error; mutations are never automatically resent after a lost reply. Earlier
runtimes that cannot be stopped retain the service’s explicit failure behavior.

Removing another row preserves the current workspace/terminal, including a
selected empty workspace. Surviving pane objects and readers are retained. The
last workspace may also be removed: the window stays empty, existing-workspace
actions disable, and New workspace remains available. It does not seed new shells
merely because the catalog became empty.

Linux UI checks cover scoped list keys, creation separation, Cancel/initial Enter,
nonselected removal, peer rename/deletion, unchanged siblings and input, removal
through the final workspace, disabled empty-state actions and creation afterward.
Native Mac interaction review remains pending.

## Find in terminal

Use Cmd–Option–S on Mac / Super–Alt–S on Linux, or **Find in terminal** in
Commands/the application menu. A compact, undimmed bar searches the selected
terminal and retained scrollback, with highlighted literal matches and a counter.
Enter/Down and Shift+Enter/Up wrap through matches; Escape/× restores the starting
view. The query stays out of PTY input and saved views. Stopped retained terminals
can be searched without reconnecting or recovering their shells. See
[Qt terminal search](terminal-search.md#qt-workbench) for limits and validation.

## External-display font comparison

`just toolkit-display-report` opens the saved Qt workbench and writes
`.local/artifacts/qt-display/report.json`. It uses the normal saved-session
launcher: closing the window detaches and keeps shells running. Use one diagnostic
window at a time; the report is replaced atomically as its state changes.

The report contains the Qt version/platform/graphics backend, logical window and
screen geometry, display/window scale, expected backing size, requested and
resolved base font face/size/weight, cell metrics, retained row-image scale and
text origins in device pixels. It contains no terminal text, working directories,
workspace titles or process details. It does not identify every fallback font used
for individual Unicode glyphs. `exact_font_match: false` alone is not an error;
font size rounding or substitution can make that flag false. Compare the resolved
family, pixel size and metrics too. Backing dimensions are calculated from logical
window size and DPR, not measured GPU texture dimensions.

For the LG/Mac comparison:

1. Open the working view, outside Overview. Choose the same font face, size and
   theme in Workbench and native Ghostty, with the same plain-text content. Avoid
   comparing a large bold prompt in one window with small regular text in another.
2. On the LG screen, wait a second after moving/resizing the window. Take an
   original-resolution screenshot and copy `report.json` to a separate LG-named
   file, before moving the window again.
3. Move the window onto the MacBook display, wait a second, and save a second
   screenshot/report pair. Repeat after unplugging the LG to review window-size
   recovery. Keep macOS display scaling at the settings used for the screenshot.

The new LG screenshot shows intact pane borders, but its different font sizes
prevent a controlled sharpness comparison. Do not infer that another rasterizer
change is needed from that image alone. These reports make scale/font mismatches
visible; they do not measure perceived sharpness or establish a rendering fix.

For a different output path, set `WORKBENCH_DISPLAY_REPORT=/absolute/file.json`
when running `just toolkit-workbench`. Ordinary launches create no report and
install no diagnostic observers. Enabled reporting is debounced after display,
geometry, attachment and font changes, outside paint/parse paths. Native Linux UI
checks exercise populated reports, font updates, resize updates and absence of
terminal content/runtime paths. Physical Mac mixed-display acceptance remains open.

## Stop all terminals

Commands and the application menu now include **Stop all terminals…**, separately
from **Close window**. Closing detaches and leaves shells/builds running. Stop all
confirms the current saved identities across workspaces/windows and keeps the
saved organization. Cancel is initially focused; terminals created after opening
are excluded, and removing a confirmed identity disables submission. Peer windows
receive stopped feedback without implicit reattachment/recovery. See
[Qt session shutdown](session-shutdown.md#qt-workbench) for limits and a disposable
Mac review procedure. Linux model and two-window UI checks cover the slice;
native Mac review remains open.


## About this client

Choose **About Terminal Workbench** in the application menu, or search for it
in Commands. Installed previews show their source revision and whether the build
included local changes. A checkout without preview metadata identifies itself as
a development checkout. **Details** shows the report; **Copy diagnostics** copies
it explicitly. Escape or Close returns focus to the selected terminal.

The report includes client build metadata, Qt/platform/architecture and local
installation, Python, configuration and session paths. It excludes workspace
names, terminal output and process environment. Review local paths before
sharing it. `configured_runtime` names the runtime configured for this client;
retained shells can still use helpers from an older preview. This report does
not inventory those helpers or decide whether an old installation can be removed.


## Live configuration workflow

**Configuration…** in the menu and **Configuration** / **Reload configuration**
in Commands expose the managed font/shortcut file and UI override locations.
Fonts and bindings follow valid file changes across open windows. Invalid files
retain the previous maps, with a visible notice; open font/shortcut drafts defer
external updates. Reload does not restart or replace terminal readers. See
[live configuration](dotfiles.md#qt-live-font-and-shortcut-configuration) for
precedence, temporary-size behavior and native Mac review steps.


## Shared workspace appearance and exited-pane actions

Appearance Apply now publishes a workspace's immutable palette or managed-default
marker to other windows in the same saved session. Preview/Cancel stays local.
An open editor rejects Apply if a peer changed the same workspace; Cancel adopts
the newer shared choice. Independent workspace files prevent unrelated edits
from overwriting each other. Existing local view snapshots remain a fallback
until a shared choice is applied. See [workspace palette persistence](dotfiles.md#qt-workspace-palettes-across-windows).

A shell exited with Ctrl+D now offers **Start new shell**, **Close pane** and
**Check again** on its recovery surface. Close pane removes only a
service-confirmed stopped entry by its stable identity, without another
confirmation. It never recovers a shell or affects neighbouring panes. Ordinary live-pane closing stays
available through the pane menu and Commands.

Managed `terminal-width = 1/1` and `3/3` now select full initial pane width.
Configuration labels that value Full width. Existing panes retain their saved
widths. The UI suite exercises actual Ctrl+D, immediate exited-pane removal
and retained sibling identity after creating a full-width terminal.

Precise horizontal trackpad scrolling follows pixel deltas directly in the pane
strip. Coarse wheel deltas accumulate while easing; keyboard navigation retains
its reveal animation. Vertical terminal scrolling and Overview are separate.

Managed `[view]` defaults now control navigation pinning and reduced motion.
Configuration shows whether each current value follows dotfiles or a saved
window choice. Use view defaults clears both overrides immediately; pin/menu
choices remain local to each saved window. See [window defaults](dotfiles.md#qt-window-view-defaults).

Horizontal gestures are received by one fixed deck viewport handler, so a scroll
sequence continues when its starting terminal moves outside the clip. A native
window-event regression crosses three full-width panes and reverses in the same
gesture; the former per-terminal receiver fails this check. This addresses swipe
stalling, independently of terminal-output throughput or native Mac load review.

## Agent details

Commands → Agent details and the pane's overflow menu open a read-only panel for
a stable terminal identity. Each Agents row also has a separate information icon;
it inspects a background agent without switching workspace, changing Overview
filters or taking terminal ownership. The existing metadata feed keeps identity
and state live. Removing its terminal closes the panel; its target never follows
an array index into another terminal.

The panel distinguishes absent/unavailable identity from a recognized agent with
Unknown state, explains Working/Needs input/Ready, and states the limits of last
reports after cancellation. Ready does not claim task success. Supported agents
have an allowlisted Copy command action for `wb-codex`, `wb-claude`, `wb-copilot`
or `wb-pi`; copying never types or launches anything. Existing policies and
invocation-local observer setup stay with the CLI. Escape/Close restores the
previous view/focus, and application shortcuts remain isolated while open.

`just toolkit-agent-info-test` covers explanations and clipboard allowlisting;
native X11/Wayland UI checks exercise live transitions, background inspection,
command/menu entry, input/grid/reader preservation and Pi recognition. Mac
interaction review remains required; use Copilot there.

## Close exited panes

After Ctrl+D or another confirmed shell exit, Close pane removes that saved pane
immediately. Its lifecycle button, pane menu and Commands share this behavior.
Running terminals retain Close terminal… and its confirmation. Unknown/unavailable
status does not qualify for immediate removal.

The new `terminal.dismiss` service method checks both runtime endpoints under the
same catalog lock as recovery. A recovered/live terminal is refused, and a
missing/unsafe/uncertain endpoint outcome cannot trigger a stop. This method
removes catalog metadata only: it executes no runtime command, reconciles no
pending destructive journal, recovers no shell and leaves lifecycle evidence
intact. It preserves the workspace when its last pane closes. Sibling identities,
readers and processes remain unchanged. A lost reply is never replayed.

An older service rejects this action locally; the client never falls back to a
live-terminal close. For a source Linux build, close the viewers, run
`just service-stop`, then `just toolkit-workbench`; retained shells stay running.
For installed previews, close their windows and activate/open the new installation.
Create a terminal, exit with Ctrl+D and click Close pane: no second question
should appear. Repeat through the menu and Commands, then check that closing a
running terminal still asks for confirmation. Native Mac interaction review
remains required.

## Find terminals and agents

Overview's Find terminals field matches workspace names, workspace directories,
reported folders, Git repositories/branches/worktrees and agent identity/state.
Space-separated terms all need to match; matching ignores case. It searches
metadata, rather than terminal output or scrollback. Existing All terminals /
Agents and All states / Needs input filters combine with the query. Clear filters
resets all three. Queries stay local to the current window and are not saved.

The normal Find shortcut (Super+Alt+S on Linux, Command+Option+S on Mac) focuses
this field while Overview is open. Up/Down selects a matching preview across
workspaces; Enter opens the selected match, or the first result if the selection
is hidden. Enter with no results does nothing. Escape clears a nonempty query;
another Escape leaves Overview. Typing or composing a query does not send input
to a terminal or activate application shortcuts.

The Agents navigation panel has its own Find agents field and All states /
Needs input toggle, with a matched/total count and explicit empty-result text.
These filters do not change Overview, workspace focus or terminal ownership.
The panel focuses search when opened. Up/Down enters the results, Home/End moves
to the first/last row, and Enter focuses its terminal. Up from the first row
returns to search. Escape in search clears filters, then closes the drawer or
returns focus to the working view when pinned. Filters follow live metadata;
unchanged result identities keep their delegates and focus.

Both searches use existing cached observers. Matching runs on query, catalog or
metadata changes, with an empty-query fast path; it adds no process scans,
filesystem reads, subscriptions, polling or work in terminal parsing/painting.
Real X11/Wayland input checks cover matching, empty results, filter isolation,
live state changes and retained catalog, reader identity and PTY input.
Native Mac interaction review remains required.
