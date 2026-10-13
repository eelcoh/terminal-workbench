# Window view persistence spike

Each launch through `just spike` leases the lowest available window slot. The
launcher prints `Window view: 1.view` (or another slot). Its client state lives in
`.local/sessions/windows/1.view`, alongside a permanent `1.lock` lease file.
Two open windows have independent slots. Closing or crashing a viewer releases
its lease; reopening reuses the first available slot and its saved view. Slots
are reusable view profiles, not saved groups of terminal owners. The spike caps
simultaneously open windows at 64.

The view records window size, independent terminal widths, active workspace, sidebar position,
focused and first visible terminal in each workspace, overview visibility,
search and filters, selected terminal, and fractional vertical/horizontal
positions. Workspace and terminal selections use stable identities. Deleted
identities fall back to the current catalog; visible ranges and scroll positions
are clamped to available content. If a reorder makes the saved first terminal
incompatible with the focused terminal, focus takes priority. Overview scroll
positions refer to the current filtered group/column order.

Window position and display assignment remain with the OS. Text selections,
terminal scrollback viewports, pending dialogs and animations are not restored.
Restoring a view never transfers ownership, restarts a shell, or changes shared
organization. A terminal still owned by another window continues to offer Move
here; restoring focus alone does not claim it.

The viewer captures view state at most once a second and writes only changes,
plus a final save on ordinary close. A crash restores the latest checkpoint,
which can lag by a second. Saves use a private temporary file, flush/fsync and
atomic replacement. Missing, malformed, truncated or unsupported versions fall
back to the default view; a successful subsequent save replaces invalid state.
Save failures go to `viewer.log` and leave the prior file intact. Runtime state
and terminal identities stay independent. There is no per-pane persistence I/O.

The inherited Unix `flock` lease follows the viewer across a detached launch,
and closes automatically when it exits. The viewer marks that descriptor close
on exec so management children do not retain it. Lock files are never removed
as part of ordinary cleanup, avoiding concurrent lock-inode replacement.
Both macOS and Linux provide the same Python/Unix implementation.

## Validation

`just check` covers private state roundtrips, Unicode search, invalid file
rejection, detached lease lifetime, concurrent allocators, crash release and
stable slot reuse. `just window-state-gui-test` exercises two real Linux windows,
close/reopen, size/width/focus, catalog reorder, input reaching the original live
shell, both overview scroll axes, query restoration, crash checkpoints, deleted
identities and corrupt-state fallback. Existing launch and multiwindow tests
cover diagnostics and ownership handoff. The owner confirmed native macOS view restoration on 2026-10-11.

On a Mac, open two windows with `just spike`, choose different workspaces and
terminals, resize each, and close/reopen one while the other stays open. Confirm
the printed slot and restored view. Open the overview, scroll both axes, close
and reopen again. Shell variables should survive; terminals owned by the other
window must still require Move here.

## Independent terminal widths

Version 2 stores each terminal's third/half/two-thirds preset and reversible full
width by stable ID, plus each workspace's working-view scroll position normalized
to the available estate. Reordering and in-window workspace moves preserve the
terminal preference; independent windows keep their own widths. New terminals
start at half width, including a workspace with just one terminal.

Version 1 files still restore window size, focus, overview and selections. Their
global split is retired: terminals start at half width, and focus is revealed in
the new strip. A subsequent save writes version 2. The old V-line divider field
is retained as a reserved compatibility field; T lines record terminal widths.
Corrupt version 2 records are rejected transactionally. Width restoration never
claims a terminal or changes a session identity.

Drag a terminal's right gap to preview a snapped guide. Release commits only that
terminal's preset; other preferences stay unchanged. Rounding can distribute a
one-pixel difference between columns to make fractional combinations fit exactly.
PTY resize requests occur at committed visible geometry, never for guide motion
or scrolling animation. Narrow columns offer rename/stop through Commands; closed
terminals retain their connection/recovery control. See [the UX brief](ux-spike.md#niri-reference-and-next-layout-slice).

## Minimal chrome preferences

Version 3 adds per-window pinned-navigation, Workspaces/Agents mode and Needs input
filter after the existing view header. Version 1 and 2 remain readable and default
to the compact rail. Invalid/truncated chrome records preserve the same all-or-
fallback contract as layout records. Only persistent presentation preferences
are saved; a temporary drawer, menu or its keyboard focus is never reopened.
Pinning changes terminal estate; opening a temporary drawer leaves it unchanged.
