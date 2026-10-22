# Qt saved-session launcher

Run `just toolkit-workbench` to open the Qt candidate on the existing saved
Workbench catalog. It uses the same `.local/sessions` directory as `just spike`;
`WORKBENCH_STATE_DIR` selects a different directory. An empty directory is seeded
by the existing session service. Existing catalogs are loaded as-is: this mode
does not add the study’s sample workspaces or rename your projects.

Closing the window detaches it. Workspaces and running shells remain in the
session service; reopening attaches to the same processes. A failed viewer also
leaves them intact. Stopped saved terminals remain stopped until you explicitly
choose **Start new shell**. **Close terminal…** confirms before stopping a live terminal. A confirmed exited
terminal offers **Close pane** for immediate metadata-only removal. Initialization does not transfer ownership: a shell
owned by another window retains its owner until you explicitly choose **Move
here…**. A second saved Qt window uses the same catalog and service.

This is an opt-in saved-session slice, not the production frontend switch.
`just toolkit-ui` still creates disposable private sessions and stops them when
its window exits. Use it for experiments that should not affect real workspaces.

Current limits:

- Workspace organization and live processes persist in the service. Qt window
  layout, focus, filters and workspace colors now persist in separate leased
  view profiles, described below. Applied workspace palettes now synchronize across session windows; preview/Cancel
  remains local. See [shared workspace palettes](dotfiles.md#qt-workspace-palettes-across-windows).
- Font defaults and application shortcuts already use the separate user config
  directory. They do not move into the session catalog.
- Workspace changes made in another window update automatically.
  **Refresh workspaces** remains available for explicit recovery.
- The existing service requires the same runtime path and project directory.
  Startup rejects a mismatched service rather than replacing it or its shells.
- Existing Unix socket length limits apply. If necessary, choose a shorter
  absolute `WORKBENCH_STATE_DIR` before creating a separate test session.

## Manual review

1. Run `just toolkit-workbench`. When another viewer owns a terminal, close that
   viewer and use Reconnect, or explicitly Move here.
2. In an available shell, set `wb_keep=still_here` and note `echo $$`.
3. Create a workspace or terminal, then close the Qt window normally.
4. Run `just toolkit-workbench` again. The workspace and terminal should remain;
   `echo "$wb_keep"` and `echo $$` should show the retained variable and PID.
5. Exit a shell normally, close and reopen the window. Its stopped entry should
   remain stopped until Start new shell is explicitly requested.

`just toolkit-saved-session-test` uses real PTYs and a temporary catalog to
verify retained organization, same shell/service PIDs across successful and
failed viewer exits, ephemeral launch-config cleanup and stopped-entry retention.
`just toolkit-ui-test` retains the X11/Wayland interaction and disposable-launcher
cleanup checks. Native Mac saved-window and migration acceptance remain pending.

## Saved Qt window views

Each saved launch leases the lowest available Qt window slot. Its profile is
`<state>/qt/windows/1.json` (or another number), with a persistent `.lock` file.
The Qt slots are separate from the spike’s `windows/*.view` format. Two concurrent
Qt windows get different slots and keep independent focus and layout. Closing or
crashing a window releases its slot; reopening reuses the first free profile.
The viewer inherits the lease descriptor so an interrupted launcher cannot free
a slot while its window still runs. At most 64 Qt slots can be active.

The view saves window dimensions, pane width fractions/full-width toggles,
selected workspace/terminal, remembered focus and horizontal position per
workspace, Overview/filter/reduced-motion state, Overview scroll and committed
workspace themes, plus the navigation pin preference. Older profiles default to
compact navigation. Layout and focus use stable IDs, not array positions. Deleted
identities fall back to the current catalog; current geometry clamps scrolling,
and selected terminals are revealed. Saved dimensions are clamped to the current
screen’s available size. Window position and display assignment stay with the OS.

Each workspace theme is saved as its name, source metadata and a bounded snapshot
of supported colors. Removing/editing its original file does not change an already
saved view’s colors. Choose the current file again in Appearance to adopt its new
colors. No commands, network requests or external file loads come from a saved
snapshot. Appearance preview is temporary: only Apply changes the committed view
choice. Source themes and the import index stay untouched.

View changes save after a short idle delay and again on normal close. No file I/O
or snapshot formatting is added to terminal parsing or per-pane painting. Invalid,
oversized, future-version or externally edited view files are left untouched,
with a visible notice; terminals remain usable. Per-window font zoom, dialogs,
selection, composition, armed shortcuts and pending confirmations are not stored.
Font defaults continue to use the separate platform UI preferences.

To review, change pane widths, select a workspace/terminal, Apply a theme and
optionally leave Overview open. Close and reopen `just toolkit-workbench`: those
choices should return while shell PIDs remain unchanged. Open a second window
to check independent slots. A stopped or remotely owned terminal stays stopped
or remotely owned: restoring focus never performs recovery or Move here.

`just toolkit-view-state-test` checks the typed bounded format and safe saves;
`just toolkit-theme-import-test` covers full theme snapshots and removed sources;
`just toolkit-saved-session-test` checks concurrent leases and slot reuse with
real retained shells. Native X11/Wayland UI tests now restart the viewer after
layout/theme/Overview changes and source removal, checking exact colors, focus,
geometry and retained reader PIDs/input. Native Mac view/display review remains
pending. The owner confirmed appearance retention across starts with `f80f00b`.

## Automatic workspace updates

Saved Qt windows follow workspace and terminal creation, rename, folder changes,
moves and removal made by other clients. Filesystem notifications invalidate the
cached catalog; a bounded, asynchronous `catalog.read` request obtains validated
service state. This is not a polling loop or a service event subscription. Reads
do not create state, start the service, reconcile creation or recover shells.

Duplicate snapshots are ignored. Local mutations pause background reads until
their confirmed reply arrives. Projection preserves terminal objects and local
layout/theme choices by stable ID; deleting an edited workspace closes its stale
editor. Existing terminal ownership never changes merely because of an update.
Malformed, timed-out or unavailable replies retain existing workspaces and show a
local notice. Terminal input remains available.

An already-running older service may lack `catalog.read`. Manual refresh remains
available. To upgrade the default organization service, run `just service-stop`,
then reopen `just toolkit-workbench`; terminal runtimes survive that service stop.
For custom state directories, stop the service for that directory explicitly.
An existing window that reported an unsupported service needs Refresh workspaces
or reopening after the upgrade.

To test, open two `just toolkit-workbench` windows. Create or rename a workspace
in one: the other should update without Refresh. Change its folder, add or move a
terminal, and remove an empty workspace. Check that existing shells remain usable
and each window retains its own appearance and focus. Moving terminal ownership
still requires the explicit Move here action.

`just toolkit-catalog-test` covers notification rearming, deduplication, local
mutation races, legacy services and bounded failure handling. The service test
checks read-only access without startup or file writes. X11/Wayland UI checks run
two concurrent windows through external catalog changes while retaining original
reader PIDs, input and ownership. Native Mac cross-window acceptance is pending.

## Older retained Linux shells

Some early saved shells expose only the replay socket, or close unsupported
snapshot requests without a reply. Qt now uses the existing viewer’s compatibility
path: ordinary replay attachment when the control endpoint is absent/refused or
closes before any response. Replay is bounded to 16 MiB and a handshake deadline;
input and terminal-query replies wait for the Ready boundary.

A rejected, partial or malformed snapshot never falls back. Older runtimes that
cannot transfer ownership retain their current viewer; close that viewer and
Reconnect. No fallback starts, stops or replaces a shell. If connection still
fails while the shell is running, the overlay shows the attachment error rather
than hiding it behind “Shell is running.” `just toolkit-terminal-legacy-test`
exercises compatibility and rejection boundaries using disposable framed sockets.
