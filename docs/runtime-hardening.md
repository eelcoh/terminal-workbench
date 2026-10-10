# Runtime hardening: terminal-state foundation

New session runtimes retain terminal state using the same pinned libghostty-vt
revision as the viewer: `f64f4aca2c29b554d111b36c3d946a9bddd159ff`. A narrow C
adapter owns one VT instance; the Rust session event loop feeds each output chunk
and applies accepted geometry changes. Only that event loop accesses the VT.
This establishes a state source independent of open windows.

## What works

- Detached cursor/status, cell-size, device-attribute and application-version
  queries receive replies through the existing bounded PTY input queue.
- Runtime text reads and binary snapshots work with or without an attached viewer.
- State continues updating after the legacy 16 MiB replay journal is exhausted.
- Binary snapshots preserve primary/alternate screens and unfinished parser input;
  decoding and continuing those snapshots are verified through the real library.
- Reading/exporting state preserves the live process, sockets and terminal geometry.

Attached legacy viewers still answer their own queries. The runtime suppresses
its replies while attached, so it does not inject duplicate responses. The viewer
transport, keyboard routing and agent metadata methods keep their current shape.
Pixel/cell metrics and client appearance preferences are not yet synchronized into
the runtime. Pixel-size reports use zero for unknown dimensions. Runtime graphics
storage is disabled; the existing viewer's graphics behavior is unchanged.

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
save format. The runtime does not yet load them when launching or reconnecting.
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

1. Negotiate snapshot-based attachment and live deltas, preserving replay fallback
   where needed; remove journal-cap attachment failure for new clients.
2. Synchronize query ownership, appearance defaults, pixel metrics and supported
   graphics state before treating restored snapshots as a full viewer replacement.
3. Introduce a session service with explicit lifecycle, exit and recovery policy.
4. Define snapshot persistence/versioning and recovery without implying that a
   stopped runtime can resurrect arbitrary processes.

This is the first slice of roadmap milestone 4, not completion of daily-use
runtime hardening.
