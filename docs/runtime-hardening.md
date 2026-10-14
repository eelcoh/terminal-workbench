# Runtime hardening: terminal-state foundation

New session runtimes retain terminal state using the same pinned libghostty-vt
revision as the viewer: `f64f4aca2c29b554d111b36c3d946a9bddd159ff`. A narrow C
adapter owns one VT instance; the Rust session event loop feeds each output chunk
and applies accepted geometry changes. Only that event loop accesses the VT.
This establishes a state source independent of open windows. New viewers now
reattach from a pinned snapshot and then consume ordered live output.

## What works

- Detached cursor/status, cell-size, device-attribute and application-version
  queries receive replies through the existing bounded PTY input queue.
- Runtime text reads and binary snapshots work with or without an attached viewer.
- State continues updating after the legacy 16 MiB replay journal is exhausted;
  new viewers can still reconnect to these runtimes through snapshots.
- Binary snapshots preserve primary/alternate screens and unfinished parser input;
  decoding and continuing those snapshots are verified through the real library.
- Reading/exporting state preserves the live process, sockets and terminal geometry.

Attached viewers, including snapshot-attached viewers, still answer their own
queries. The runtime suppresses its replies while attached, so it does not inject
duplicate responses. Existing input frames, keyboard routing and agent metadata
methods keep their current shape.
Pixel/cell metrics and client appearance preferences are not yet synchronized into
the runtime. Pixel-size reports use zero for unknown dimensions. Runtime graphics
storage is disabled; attached viewers keep their live graphics budgets. Images
are not restored from runtime snapshots.

## Window reconnection

The viewer first tries `ATS1` on the private control endpoint with the exact VT
revision. A matching runtime captures state and registers the attached stream in
one event-loop turn. The socket writer sends bounded snapshot chunks, a finish
marker and READY, then live output/resize events queued after that capture. Input
waits for READY. No historical VT queries are replayed during restoration.

The client caps the snapshot at 64 MiB, validates the Ghostty checksum/finish and
4096-byte continuation limit, then replaces its initial VT and restores client
callbacks, theme defaults and pixel metrics before processing live bytes. OSC
color overrides survive application of theme defaults. Existing live graphics
budgets are re-enabled, but runtime snapshots do not retain images; graphics
restoration and detached appearance/pixel queries still require follow-up work.

Older control endpoints close an unknown method; the viewer then connects through
the existing replay socket. Missing control sockets also use this fallback.
Malformed/partial snapshot responses and revision mismatches fail locally and do
not silently switch transports or restart a shell. A different VT revision needs
an explicitly compatible attachment method. Existing live runtimes are never
upgraded automatically; create a new terminal after rebuilding to try snapshots.
The client allows ten seconds for initial attachment; socket writes have a
two-second deadline and queued live output remains bounded to 256 frames.

## Shared organization and window ownership

All windows launched against the same state directory share its workspace and
terminal catalog. The existing management CLI serializes writes under a file
lock and atomically replaces the catalog projection. Each window checks that
projection four times per second and applies changes by stable identity, retaining
live connections, terminal state, focused terminal, workspace selection and
overview scroll/selection.
Focus falls back to a surviving neighbor if its terminal or workspace is removed.
Refresh waits for editing, selection, drag and transition gestures to finish.

New terminals belong to the window creating them, even if another window sees
the catalog first or another creation commits before the creator refreshes. The
management result identifies exactly the created terminal and workspace. Other windows show “Open in another window”. Enter a terminal
from the overview and click “Move here” in its terminal view to transfer ownership
without restarting its shell. The old window receives a revocation and offers the
same action. Each terminal has one owner; moving one does not move its siblings.
The transfer restores the current runtime snapshot and then accepts live input.

Earlier runtimes do not support ownership transfer. Replay-only connections
are recognized on initial attachment and offer Reconnect immediately. Older
snapshot runtimes without transfer support are recognized after a failed transfer;
an ordinary attachment preserves the current owner. Reconnect asks you to close
the owning window first. Attachment diagnostics identify the session socket and
requested method so mixed runtime generations can be distinguished in viewer.log.
New terminals
created after building use the new runtime and support direct transfer. Existing
shells are never killed or automatically upgraded to enable this feature.

Shared organization now runs through an automatically started Python session service
using the existing catalog authority. Rust runtimes continue to own PTYs and input
attachments. Service restart adopts live endpoints; stopped saved terminals require
explicit recovery through the CLI or the graphical confirmation. See [session-service lifecycle and validation](session-service.md).
This remains the spike integration, not a final installed daemon.
Independent window views now persist separately from runtime state; native Mac
restoration is owner-confirmed. See [window state](window-state.md).
Named session selection, cross-window appearance
updates and restart-after-reboot policy remain follow-up work.

`just multiwindow-gui-test` verifies real two-window creation, renaming, terminal
movement and workspace closure, independent focus, creator ownership under overlapping saves, transfers
in both directions and surviving shell PID/state/socket identity. Runtime tests
also verify rejection before revocation and stale-owner input/resize isolation.
Launching a second window from a Workbench terminal starts it independently,
without keeping the launcher in that shell's foreground or teeing viewer logs
into its PTY. The two-window test also launches a real viewer from the terminal
being transferred and verifies continued shell input before and after handoff.
Native macOS acceptance remains pending.

## Build and inspect

`just runtime-build` prepares the pinned VT library and Rust executable without
CMake, Raylib or a graphical desktop. It needs Rust, Python, Git, a C compiler and
Zig 0.16.0. The existing compiler-discovery wrapper sets the child environment.
Source cloning uses SSH and can reuse the viewer's clean pinned Ghostty checkout.
`just check` prepares this dependency before validating Rust and runtime behavior.
`just spike-build` also prepares it before compiling the runtime.

For a newly created terminal, its shell exposes its socket path in
`WORKBENCH_SESSION_SOCKET`. From another terminal, using that absolute path:

```sh
target/debug/workbench-session read /absolute/path/to/terminal.sock
target/debug/workbench-session snapshot /absolute/path/to/terminal.sock terminal.snapshot
```

Text is the active terminal screen's plain representation, including retained
history, independent of client viewport scrolling. Snapshot files use the pinned
Ghostty binary format and are experimental artifacts, not a durable cross-version
save format. The runtime does not load files when launching; window reconnection uses a
fresh in-memory snapshot of the live runtime.
Existing live runtimes retain their old executable and behavior after rebuilding;
create a new terminal to exercise these endpoints. Missing methods produce a
local CLI error and leave the existing session running.

## Bounds and verification

Scrollback is limited to approximately 1 MiB and 1000 physical lines; Ghostty
prunes at page granularity, so these limits include a documented page-sized delta.
Unfinished-sequence retention is capped at 4096 bytes. The query-reply buffer is
64 KiB; PTY input/event queues keep their existing bounds. Text/snapshot exports
are capped at 64 MiB. Exports run on the session event loop and control writes
have a one-second deadline; snapshot streaming without pausing this loop is
future work. No filesystem I/O or snapshot encoding enters viewer rendering.

`just snapshot-gui-test` verifies the real viewer after more than 16 MiB of output,
primary/alternate screen restoration, unfinished parser continuation, copied text,
live query replies, same-PID reconnect and fallback to an older control endpoint.
The fallback test also exercises Linux connection resets from unread handshake
bytes. Backend tests check revision rejection, exclusive attachment, snapshot
framing, idle connections and snapshot/live ordering.

Linux checks cover real detached queries, reply ownership when attached, state
reads without stealing the viewer connection, primary/alternate state, incomplete
VT sequences, corrupt snapshot rejection and state export after 17 MiB of output.
Existing stop, agent, recovery, catalog and graphical input/selection/clipboard
regressions pass. Native Mac validation of this runtime slice remains pending.

`just runtime-core-scale` is a Linux diagnostic profile with one and fifteen
runtimes at fixed 80×24 geometry, each processing 64 MiB of styled text while
detached. It verifies the final marker through the state endpoint and records
`.local/test-results/runtime-core-scale.json`. The measured CPU includes PTY reads,
channel/framing/history work, VT parsing and final state reads; it is not VT-only.

| Runtimes | Wall seconds | Aggregate runtime CPU seconds | CPU seconds per runtime | Final runtime RSS |
| --- | --- | --- | --- | --- |
| 1 | 0.525 | 0.59 | 0.590 | 8.9 MiB |
| 15 | 4.394 | 12.17 | 0.811 | 7.8–23.7 MiB |

Per-runtime CPU increased about 37.5% in this single concurrent run. Scheduling,
read chunking and allocator retention affect this diagnostic; it is not evidence
of constant scaling, an improvement over the previous runtime, or native GUI
frame smoothness. Longer repeated measurements and a pre-change comparison remain
appropriate before performance acceptance.

## Remaining hardening

1. Improve snapshot transfer/decoding responsiveness and lifecycle feedback;
   the bounded complete-snapshot attachment is implemented, with legacy fallback.
2. Synchronize query ownership, appearance defaults, pixel metrics and supported
   graphics state before treating restored snapshots as a full viewer replacement.
3. Extend the initial organization service with creation/close
   journaling, subscriptions and installation lifecycle policy. Confirmed exit
   evidence is now recorded by new runtimes; unrecorded losses remain unknown.
4. Define snapshot persistence/versioning and recovery without implying that a
   stopped runtime can resurrect arbitrary processes.

These are the terminal-state and reattachment slices of roadmap milestone 4,
not completion of daily-use runtime hardening.

## Runtime lifecycle evidence

New invocations persist bounded atomic lifecycle records and publish final exit
evidence before viewers observe EOF. The organization service exposes optional
details without changing legacy status/terminal transport or taking attachment
ownership. Startup/recovery resets previous evidence, including when launching
an earlier runtime. The UI uses confirmed facts for stopped-pane feedback and
keeps unrecorded disappearance distinct from a known shell exit.
See [stopped-shell reasons and validation](terminal-lifecycle.md).
