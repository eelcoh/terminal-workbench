# Window view persistence spike

Each launch through `just spike` leases the lowest available window slot. The
launcher prints `Window view: 1.view` (or another slot). Its client state lives in
`.local/sessions/windows/1.view`, alongside a permanent `1.lock` lease file.
Two open windows have independent slots. Closing or crashing a viewer releases
its lease; reopening reuses the first available slot and its saved view. Slots
are reusable view profiles, not saved groups of terminal owners. The spike caps
simultaneously open windows at 64.

The view records window size, split width, active workspace, sidebar position,
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
close/reopen, size/split/focus, catalog reorder, input reaching the original live
shell, both overview scroll axes, query restoration, crash checkpoints, deleted
identities and corrupt-state fallback. Existing launch and multiwindow tests
cover diagnostics and ownership handoff. Native macOS view restoration still
needs manual acceptance.

On a Mac, open two windows with `just spike`, choose different workspaces and
terminals, resize each, and close/reopen one while the other stays open. Confirm
the printed slot and restored view. Open the overview, scroll both axes, close
and reopen again. Shell variables should survive; terminals owned by the other
window must still require Move here.
