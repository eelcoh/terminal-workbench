# Session service spike

The launcher and management CLI now share one background organization service per
state directory. It starts automatically and concurrent launches reuse it. Python
hosts this first service slice using the existing catalog module and its file lock;
Rust runtimes still own individual PTYs, terminal state, attachment and agent signals.
No new dependency or renderer integration is required. This is a prototype service,
not the final installed daemon or completion of runtime hardening.

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
Stopped entries remain visible; the initial recovery interface is the CLI above.
A graphical recovery flow with confirmation and detailed exit reasons is follow-up.

## Protocol and compatibility

The private `service.rpc` Unix stream endpoint carries one bounded newline-terminated JSON request
and reply per connection. Protocol 1 exposes catalog operations, `health`,
`terminal.recover`, `session.recover` and service shutdown. Requests are serialized
and catalog writes retain the existing lock and JSON/TSV projection semantics.
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
retrying. Runtime startup and metadata persistence are not one transaction; a
service crash during creation can leave an orphan runtime before metadata commit.
Creation/close journaling, subscriptions, background health snapshots and graceful
application-wide shutdown policy remain further hardening. Removing a disposable
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
Selected stopped-terminal recovery still needs manual macOS acceptance.
