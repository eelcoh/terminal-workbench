# Stopped-shell reasons

New runtimes record lifecycle evidence independently of desktop windows. When a
saved terminal is stopped, its footer and **Start new shell** confirmation can
show a normal exit, a nonzero exit code, a signal termination, an explicit
Workbench stop, or a recorded startup/runtime error. `just session-status` shows
the same reason through the shared organization service.

Closing a window still detaches live shells. Explicit recovery still creates a
fresh shell under the same terminal identity and appearance; it cannot resume old
jobs, shell variables or running commands. The normal Stop terminal/workspace
actions still remove their entries. Where a stopped saved entry remains, its
reason is available without starting a shell or taking another window's owner.

## Try it

After rebuilding, create a **new terminal** and type `exit 42`. Its footer should
say **Shell exited with code 42.** Choose **Start new shell** to see the reason
in the confirmation. Cancel keeps it stopped; explicit confirmation starts a new
shell and clears the previous exit evidence. `exit` records a normal code-0 exit.
Older running runtimes keep their existing behavior and usually have no receipt;
they are not killed or upgraded merely to enable richer feedback.

The launcher updates an earlier organization service when needed, preserving
live runtime processes. Read-only background checks do not start a stopped
service. The optional `terminal.lifecycle` method falls back through the unchanged
`terminal.status`/health methods on an older service, with an honest unknown reason.
The legacy `STATE` CLI line and old terminal transport remain unchanged; the new
presentation uses a separate `LIFECYCLE` line with a bounded human-readable message.

## Evidence and bounds

Each invocation writes a version-1 JSON sidecar beside its socket, named
`ID.lifecycle`, containing a start timestamp, runtime/shell PIDs, event, and
available exit code or signal description. This is runtime/session data, not
client presentation or dotfile configuration. It is retained after the runtime
ends and can be read after organization-service restart. It is not a process
snapshot and cannot authorize replacing a live endpoint.

The catalog first invalidates the prior invocation's evidence even when launching
an older runtime. New runtimes then publish starting/running/final records. Writes
use private temporary files, mode 0600 and atomic rename, with file synchronization
and best-effort directory synchronization. Startup requires an initial record;
subsequent record-write failures are logged and leave conservative evidence.
Final evidence and endpoint cleanup precede the viewer's ordinary EOF.

Readers accept at most 4096 bytes from a user-owned regular file, reject writable
by-other/symlink/FIFO/invalid/incomplete records, and preserve normal recovery when
evidence is missing or malformed. Unsafe sidecar paths are refused during a new
launch instead of redirecting writes. Signal descriptions come from the existing
PTY library; numeric signal IDs are not inferred from those strings. Messages
are bounded and sanitized before presentation. Records are read during explicit
health/status work; no filesystem discovery is added to rendering or PTY parsing.

An unfinished running record plus an unavailable endpoint means **Runtime
disappeared; no shell exit was recorded.** This cannot distinguish a runtime
crash, forced termination, reboot or lost evidence. Older/missing records remain
**no exit reason recorded**. Listening or uncertain endpoints retain their
existing running/unavailable states, regardless of old receipt contents.
SIGKILL, abort or sudden machine loss can prevent the runtime from writing a
final record; the UI does not guess the missing cause.

## Validation

The Linux build and `just check` cover real code-0/code-42 exits, shell signal
termination versus an explicit stop, runtime SIGKILL with unrecorded exit,
service adoption, fresh recovery, legacy launch invalidation, startup failure,
private/unsafe metadata paths and optional-method fallback. Native Mac build and
visual acceptance of the exit-code footer and restart confirmation were confirmed
by the owner on 2026-10-14; broader Mac signal/crash checks remain pending. Use `just lifecycle-gui-test` to exercise the
footer, confirmation, cancellation/Enter guards and fresh-shell recovery with
live siblings. Captures are under `.local/artifacts/lifecycle/`.

Creation/close intent is now journaled; see [catalog transactions](catalog-transactions.md).
Explicit session shutdown is now implemented; see [session shutdown](session-shutdown.md).
Installed-app lifecycle, durable terminal snapshots,
subscriptions and installed-daemon lifecycle remain separate hardening work.

The Linux lifecycle GUI and existing management GUI checks pass: stopped-pane
and confirmation reasons, guarded cancellation/Enter, explicit fresh-shell
recovery, preserved sibling PIDs/catalog, and no automatic shell resurrection.
