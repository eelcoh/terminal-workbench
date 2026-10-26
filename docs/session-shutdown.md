# Session shutdown

Closing a Workbench window detaches its terminals, and shells keep running.
**Stop all terminals** is an explicit separate action in Commands and the
Workbench menu. It stops the confirmed terminals across all workspaces/windows
in this shared session, while retaining their saved organization for later use.

The confirmation shows its terminal count and explains that running shells and
foreground jobs stop. Workspaces, names, folders, themes and window layouts stay.
Cancel/Escape preserve shells; opening the confirmation and pressing Enter does
not confirm. Use Tab to select Stop, then Enter, or click Stop. Terminals created
after the confirmation opens are excluded. If a selected identity was removed,
the operation fails before stopping anything: reopen the confirmation.

Stopped entries show their recorded reason and offer **Start new shell** after
the usual explicit confirmation. They do not resume arbitrary jobs, shell
variables or exited processes. Shutdown does not close windows or stop the
organization service; it leaves an inspectable session. Stop terminal / Stop
workspace retain their existing removal behavior.

The new service method `session.stop` requires a nonempty, unique `terminal_ids`
list of saved identities, bounded by the catalog's 256-terminal limit. The
method is advertised independently; legacy methods/transport keep their shape.
The launcher upgrades an older organization service, adopting its existing shell
runtimes. Rebuilding does not replace live runtimes. A legacy live runtime without
both stop endpoints, or an unsafe/uncertain endpoint, rejects the initial plan
before any stop. Already-stopped targets are harmless to include; their existing
exit evidence is preserved.

The catalog saves validated `pending_stop` intent before stopping anything.
Requests remain serialized under the organization lock. Up to 32 bounded workers
call the existing runtime stop command with 15-second subprocess deadlines;
no work is added to rendering or PTY byte handling. Endpoint status is checked
again before each stop. An individual failure leaves the full intent for retry;
some confirmed targets may already be stopped. Initialization and subsequent
mutations/recovery finish authorized pending shutdown before proceeding, without
starting replacement shells. Unknown IDs, duplicate/empty/conflicting intent,
non-sockets and unowned endpoints remain errors. A lost reply is not retried
automatically; inspect terminal status before trying again.

The viewer runs management asynchronously. Catalog publication invalidates cached
status for disconnected panes, including those owned by another window, so they
can show stopped feedback and offer recovery. Live attachments retain their
normal EOF/status path. A running window still owns only its own presentation
state; confirmation IDs are transient client input, not persisted UI state.

`just check` includes one/fifteen-terminal selected shutdown, stable catalog/themes,
same-PID/variable/socket preservation for an excluded sibling, repeated shutdown,
service restart, explicit recovery, preflight rejection and interrupted/failed
intent reconciliation. One Linux diagnostic sample took 0.054s for one selected
terminal and 0.071s for fifteen, about 31% more end-to-end wall time; this includes
service requests and parallel stop processes, not a renderer or throughput benchmark.

`just shutdown-gui-test` covers two real Linux windows, Cancel/unfocused Enter,
new-terminal exclusion while confirmation is open, other-window stopped feedback,
explicit recovery and unchanged close-detach behavior. Captures are under
`.local/artifacts/shutdown/`. The owner confirmed native Mac shutdown/restart
on 2026-10-14.

For Mac review, first try Cancel in your normal session. For a confirmed stop,
use a disposable state so no daily-use jobs are affected:

```sh
BAND_STATE_DIR=/tmp/wb-shutdown-review just spike
```

Create some terminals, open Commands (Command+Option+P), choose Stop all terminals,
then confirm. Names/workspaces should stay, and the selected shells should show
Stopped by Workbench. Restart one through Start new shell. Close and reopen with
the same command: restarted shells stay live and others remain stopped.
The matching backend test also uses disposable state:

```sh
TMPDIR=/tmp python3 tests/session_shutdown_test.py target/debug/band-session
```

Installable app/daemon lifecycle, session selection and update policy remain open.

## Qt workbench

The Qt candidate now exposes **Stop all terminals…** through Commands and the
application menu. It reuses `session.stop`, with no new runtime or transport
operation. **Close window** continues to detach: builds and shells can keep
running after the window disappears. Stop all is a separate explicit action.

The dialog captures the current saved terminal IDs, including terminals in other
workspaces or owned by another window. It states that shells/foreground jobs stop
and organization/appearance remain. Cancel receives initial focus; Enter initially
cancels. New terminals are excluded. Removing a confirmed ID disables Stop with a
request to cancel and reopen, rather than targeting another terminal. Busy state
blocks repeated submission and dismissal. Errors remain in the confirmation and
mutations are never automatically resent after a lost reply. An older service
that lacks `session.stop` rejects this action without changing other features.

Disconnected peer windows now refresh their read-only lifecycle cache after a
validated filesystem-triggered catalog read, even when saved organization is
unchanged. Already live readers keep their normal EOF path. An invalidation during
an in-flight status request coalesces one fresh read afterward, so an old reply
cannot permanently hide the new stopped state. There is no periodic polling,
implicit reconnect, service startup or shell recovery in this path. Stopped
feedback takes precedence over an obsolete “Open in another window” notice.

`just toolkit-catalog-test` and `just toolkit-lifecycle-test` cover unchanged
snapshot invalidation and in-flight read ordering. `just toolkit-shutdown-gui-test`
uses real disposable terminals and two Qt windows on X11/Wayland: menu/Commands
handoff, Cancel/initial Enter, stale IDs, cross-window ownership and stopped
feedback, newly-created-terminal exclusion with unchanged shell PID/variable,
retained catalog/readers, explicit recovery and unchanged close-detach behavior.
The full `just toolkit-ui-test` includes the same flow and empty-session action
availability. Captures are under `.local/artifacts/qt-ui/{xcb,wayland}/session-stop*.png`.
Native Mac review remains required.

For Mac review without affecting daily-use terminals:

```sh
BAND_STATE_DIR=/tmp/wb-qt-shutdown-review just toolkit-workbench
```

Create some terminals and a second window with the same command. Test Cancel,
then confirm Stop all. Both windows should show stopped entries while keeping
workspaces. **Start new shell** is explicit; closing either window keeps surviving
shells running. This validates the Qt client independently of the previously
confirmed prototype shutdown flow.
