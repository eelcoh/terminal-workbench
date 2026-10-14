# Sustained-session checks

`just session-soak` starts a disposable catalog with fifteen independent runtimes
for five minutes. It uses temporary state and foreground fixture TUIs, never your
saved terminals or real agents. Builds use the existing pinned Ghostty dependency.
The runner needs the same C compiler/Python prerequisites as runtime builds; a
small decoder links the existing runtime adapter and real Ghostty library.

```sh
just session-soak             # 300 seconds, 15 terminals
just session-soak 60 1        # shorter single-terminal case
just session-soak 60 15       # compare the populated case
```

On this Linux workstation, use `distrobox enter build-machine -- just --justfile
/var/home/eelco/Code/ideas/terminal-workbench/justfile session-soak 300 15`.
On Mac, use `TMPDIR=/tmp just session-soak 300 15` to keep Unix socket paths short.
The native Mac soak has not yet been validated.

The requested interval begins after startup; cleanup and at least two complete
cycles can extend total execution. Existing protocol/query/subprocess deadlines
detect lost or stalled operations. This is not a frame-time or CPU benchmark.

Each TUI writes repeated styled text, alternates primary/alternate screens and
records a bounded progress counter. During each cycle the test:

1. Detaches every presentation stream and checks runtime-owned cursor/size replies,
   including absence of duplicate replies and the last accepted geometry.
2. Reattaches through a negotiated snapshot, decodes it with the actual pinned
   Ghostty adapter and checks retained workload text, then verifies new live output.
3. Transfers ownership to another client, observes revocation and attempts stale
   input/resize. The replacement receives its accepted resize and output; stale
   owner input must never reach the TUI.
4. Periodically stops or SIGKILLs only the organization service and adopts surviving
   runtimes, preserving catalog, fixture preferences and runtime socket inodes.

At the end, every fixture returns to its original shell. The test checks shell PIDs
and invocation-local variables, retained runtime text and absence of stale input.
It joins client readers and explicitly stops its disposable runtimes/service.
These assertions use observable protocol/process facts, not arbitrary timing
thresholds for performance. Ctrl+C follows the runner’s cleanup path.

JSON reports are written to `.local/test-results/session-soak-1.json` or
`session-soak-15.json` (matching the terminal count). They contain pass/failure,
elapsed duration, cycles, attachment count/max transfer latency, fixture-output
bytes and runtime resource samples. Diagnostics retain the baseline and at most
255 recent samples. RSS is sampled through `ps` on Unix; descriptor counts are
reported when Linux `/proc` is available. They are observations, with no universal
memory-leak threshold: VT/history allocation, replay-journal warmup/pruning and
allocator retention can change RSS. The harness also uses output files/decoder
processes, so numbers should not be presented as renderer or production throughput.
Failure logs contain only the disposable fixtures' bounded log tails.

`just check` includes a three-second single-terminal case and stores a separate
`session-soak-check.json`. Longer soak checks remain explicitly invoked rather
than adding minutes to every ordinary validation run. One/fifteen-terminal Linux
results are recorded below.

This does not prove hours of real Claude/Codex/Copilot operation, native Mac or
Wayland input/rendering, suspend/resume, GPU behavior, graphics restoration,
cross-version durable snapshots or installation/update lifecycle. Those remain
roadmap gates. Native Mac review can run the same disposable recipe without
stopping daily-use terminals.

## Linux results (2026-10-14)

| Terminals | Interval | Full cycles | Service restarts | Snapshots decoded | Fixture output | Max snapshot transfer |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 60.2s | 194 | 78 | 389 | 10.5 MiB | 2.6 ms |
| 15 | 302.0s | 267 | 108 | 8025 | 908.7 MiB | 10.5 ms |

Both runs passed all identity, live-output, decoded-snapshot, query ownership,
geometry, stale-owner, organization and final-shell-variable assertions. The
fifteen-terminal run produced about 909 MiB of aggregate fixture output. Runtime
descriptor observations started and ended at 10 per process in both cases.
The populated runtime RSS range moved from about 7.5–7.7 MiB at startup to
20.6–27.0 MiB at completion, with a sampled peak of 27.9 MiB. The one-terminal
runtime moved from about 7.6 to 21.3 MiB. These samples include warmup/journal
allocation and allocator retention; they do not prove absence of memory leaks.
Intervals differ, so this table is coverage evidence, not a scaling comparison.
The Ctrl+C check also passes and verifies failure reporting and dead fixture runtimes.
