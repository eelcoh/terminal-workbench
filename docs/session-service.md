# Session service spike

The launcher and management CLI now share one background organization service per
state directory. It starts automatically and concurrent launches reuse it. The
service is `band-session service` (`crates/runtime/src/service/`), a Rust port
of the earlier Python server with the same protocol, catalog files and locks. The
Python server and catalog were removed (2026-10-23); the service is the only
implementation. Separate runtime processes still own individual PTYs, terminal
state, attachment and agent signals. `scripts/session_service.py` is a client
for development tools and tests (`just session-status`, `recover-sessions`,
`recover-terminal`, `service-stop`). This is not yet the final installed daemon.

Closing a window detaches its terminals. Stopping or crashing the organization
service leaves their shells running. The next request starts a replacement service
and adopts listening endpoints without changing terminal identities, shell PIDs,
terminal state, socket inodes or saved appearance. Attachment ownership remains
runtime-authoritative: Move here and legacy Reconnect keep their existing behavior.

## Try it

Run `just spike` twice. Both launchers should print the same Session service PID.
The service writes diagnostics to `.local/sessions/service.log`.

- `just session-status` lists terminal IDs, workspace names, endpoint health and
  runtime compatibility. It does not attach to a terminal or start a shell.
- `just service-stop` stops only the organization service. Existing windows and
  terminals continue. The next management action or launch starts it again.
- `just recover-terminal ID` starts a replacement shell only for a stopped terminal.
  Its identity, name and theme remain; its old processes and shell variables cannot
  be recovered. Recovery refuses a listening endpoint and preserves live siblings.
- `just recover-sessions` is the existing explicit bulk recovery command. It now
  goes through the service and starts replacements for stopped saved terminals.

Fresh state directories get initial workspaces and terminals. Once a catalog exists,
opening a window preserves stopped entries instead of silently starting replacement
shells. This includes terminals whose socket vanished after shell exit or reboot.
Stopped entries remain visible. The graphical client now checks the service when
a disconnected terminal becomes visible and offers **Start new shell** only for a
confirmed stopped runtime. Its confirmation preserves the terminal name/theme
and explains that prior jobs do not resume. Cancel leaves the entry stopped.
A service-unavailable or uncertain endpoint offers **Check again**, not recovery.
Live terminals keep ownership-aware **Move here** / **Reconnect** actions.
Status checks are asynchronous for visible disconnected terminals, once per disconnection, and do not start a stopped
service. Older running services fall back to their existing readonly health method.
A cached stopped result is rechecked by the recovery operation: if another caller
has already started a shell, the operation refuses to replace it. After recovery,
the client attempts an ordinary attachment, without taking another window's owner.
New runtime lifecycle records now supply confirmed exit codes, signal termination
and explicit-stop evidence to the footer, recovery confirmation and status CLI.
Missing/unrecorded exits remain unknown; crash versus reboot is not inferred.
See [stopped-shell reasons](terminal-lifecycle.md).

## Protocol and compatibility

The private `service.rpc` Unix stream endpoint carries one bounded newline-terminated JSON request
and reply per connection. Protocol 1 exposes catalog operations, `health`,
optional `catalog.read` (readonly), `terminal.status` (readonly),
optional `terminal.lifecycle` (readonly), optional
`session.stop` (explicit terminal IDs, organization retained), `terminal.recover`, `session.recover` and service shutdown. Requests are serialized
and catalog writes retain the existing lock and atomic JSON replacement.
PTY input, snapshots and rendering do not travel through this service. Focus,
selection and layout remain local to each window. There is no frame-time polling.

New Rust runtimes answer `CAPS` on their control endpoint with a protocol identifier,
PID and supported methods. Health checks cache capability replies by socket identity.
Older runtimes without a control endpoint are reported as legacy replay; control
endpoints without a capability reply are reported as unadvertised. An unadvertised
runtime may support snapshots or transfer; the service does not guess. Rebuilding
never upgrades an already-running runtime. A new terminal uses the rebuilt binary.

The state directory must be user-owned and not writable by other users. The service
endpoint is mode 0600. Startup locks prevent duplicate authorities. Only a refused,
user-owned stale socket is removed; regular files, symlinks, live listeners and
uncertain connection failures are preserved. A protocol/configuration mismatch is
reported locally and requires an explicit service stop before replacement.

A client retries only before establishing a connection. It never automatically
replays a mutation after sending it: if the reply is lost, inspect the catalog before
retrying. Creation saves stable identities and pending intent before spawning any shell.
Reopening reconciles that intent, adopts survivors and leaves absent launches
stopped. Interrupted closes complete before further mutations or recovery;
validated intent prevents incomplete workspace-removal plans from stopping shells.
Health optionally advertises `features: ["catalog-journal-v1"]`; the launcher
updates an older organization service while keeping its terminal runtimes alive.
See [catalog transactions](catalog-transactions.md). Subscriptions, background
health snapshots and installed-app lifecycle remain further hardening. An explicit
confirmed terminal shutdown is now available; see [session shutdown](session-shutdown.md). Removing a disposable
state directory lets its service exit; it never kills independent terminal runtimes.

## Validation

`just check` includes a real service test covering simultaneous startup, service
reuse, advertised capabilities, orderly stop and SIGKILL with a live attached shell,
same-PID adoption, stopped identities across service restart, individual recovery,
concurrent creation, lost-reply isolation, the reported Mac directory length and
malformed request/unsafe endpoint isolation.

`just multiwindow-gui-test` and `just management-gui-test` exercise the service through
real windows and the management CLI. Linux checks use X11/Xvfb. Native macOS service
startup/reuse, shared PID and stop/live-adoption were confirmed on 2026-10-11.
Graphical stopped-terminal recovery was also confirmed by the owner on 2026-10-11.

## Read-only catalog access

Optional advertised method `catalog.read` accepts only the protocol and method.
It returns the validated existing catalog under its lock, without initialization,
reconciliation, saves, runtime discovery or shell recovery. The request helper
never starts a missing service for this method. Qt saved windows use this
method after filesystem notifications, keeping background observation separate
from explicit mutations and refresh/recovery. Older services can reject the method
without affecting terminal runtimes or manual refresh.


## Instance-checked service retirement

Optional advertised method `service.retire` accepts `protocol`, `method` and
`expected_instance`. The expected value is the `service_instance` string from
`health`: a fresh 32-character UUID hex value for each authority process.
Missing or mismatched values reject without retiring the current service. A
successful reply includes `retired: true` after the listener is retired and its
lock released, using the existing shutdown acknowledgement barrier. PTY runtime
processes and shells are independent and remain running.

The new health field is optional for older clients; the protocol number remains
1. The existing `service.shutdown` method retains its behavior and parameter
shape. An absent service is never started to handle a retirement request. Clients
must advertise-check the new method; they must not replace it with unconditional
shutdown when the service lacks support.

The staged-preview launcher uses this runtime method for explicit `--activate`,
with client-owned window lease exclusion. The service does not know about Qt,
window slots or app bundles. Activation's slot checks/reservations, selected paths
and update commands live entirely in the installed client layer. See
[preview updates](qt-staged-preview.md#switch-preview-builds).

## Remove a stopped terminal

`terminal.dismiss` accepts a stable terminal identity with the normal service
protocol/runtime/directory fields. It removes only a stopped terminal's catalog
entry, preserving its workspace and runtime evidence. Endpoint ownership/type and
liveness are rechecked inside the catalog lock shared with recovery. Live or
uncertain endpoints, missing identities and pending operation journals reject
without stopping, recovering or reconciling anything. It runs no runtime command.
Existing `terminal.close` retains its explicitly confirmed stop-and-remove contract.
Clients must not substitute it when dismissal is unavailable or rejected.
