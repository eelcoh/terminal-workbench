# Qt performance with live runtime sessions

Run `just toolkit-live-perf` on Mac or Linux for native measurements. Keep its
window exposed on one display; do not resize, minimize or move it during the
run. It opens fresh benchmark windows automatically, uses disposable private
sessions, and stops those sessions afterward. No existing Workbench catalog or
terminal is used. Allow a few minutes for building and six trials.

On Linux, `just toolkit-live-perf-test` runs twelve trials on private Xvfb with
software OpenGL: one/fifteen panes, 1x/2x DPR and three fresh processes per case.
Native runs use the display's current DPR and graphics backend. Logs record
Qt's graphics details; JSON records platform, API, DPR, source revision and
whether tracked files were modified. Artifacts are respectively
`.local/artifacts/qt-live-performance/` and `.local/artifacts/qt-live-test/`.
Each contains `report.json`, per-trial JSON/logs and a final window PNG.

## Workload and correctness

Each source terminal is 80×24. Fifteen sources are scaled into a 5×3 scene in a
1120×680 requested window; source grids stay full resolution. The initial
compositor-assigned window geometry is recorded and then held constant.
The Qt client is built Release; the existing runtime-build recipe supplies the
**debug runtime**, so runtime CPU numbers are diagnostic, not release budgets.

Each trial runs idle, one-row overwrite, full-screen overwrite, full-screen with
only one pane visible, then full-screen with all panes hidden. Every active
producer emits sixty changing ASCII generations paced over one second. The
final row carries a unique generation/pane marker. Startup and initial snapshot
transfer are outside scenario timings. Cursor blinking and production chrome
are absent. Intermediate updates can coalesce; this is not a throughput limit,
a 60-fps guarantee, an input latency test or a Unicode workload.

Exact producer byte counts must reach each attached client, refresh its grid,
and, for visible items, reach paint. Both the client viewport and authoritative
runtime read must contain each final marker. Hidden panes must perform zero
raster/paint work while continuing to consume output. Reveal must raster exactly
24 current rows per previously hidden pane. Idle must receive no output and do
no raster/paint work. These deterministic checks gate success; wall-clock or CPU
thresholds do not.

## Measurements and limits

Viewer process CPU comes from `std::clock`, including its render threads.
Scenario wall time includes pacing and catch-up. Summed paint wall time measures
only the painted-item callback; it is distinct from process CPU. Swap
notifications do not prove GPU completion. GPU time, texture uploads and VRAM
are unmeasured.

Checkpoint CPU and RSS are collected separately for the viewer, runtime group
and producer group. Linux uses procfs; Mac uses `ps` time/RSS. Quantization bounds
are retained with each sample; small runtime deltas may fall below resolution.
Mac RSS units and CPU formatting follow [Apple's ps implementation](https://raw.githubusercontent.com/apple-oss-distributions/adv_cmds/main/ps/print.c).
Group RSS sums can count shared pages more than once; these are endpoint samples,
not continuous peaks. Viewer lifetime peak RSS comes from wait4 and also includes
startup and the final screenshot readback. Producer polling CPU is separate.
Row image bytes count cached foreground images, not all backing surfaces/textures.

## Linux diagnostic baseline, 2026-10-17

Qt 6.11.2, xcb, Xvfb/software OpenGL. Three fresh trials per case; values below
are medians. This checkout includes the benchmark changes over 2ed6c10. All
12 trials passed byte/marker, hidden/idle and reveal checks.

| DPR | Panes | Idle CPU s | One-row CPU s | Full CPU s | One visible CPU s | All hidden CPU s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1x | 1 | 0.004 | 0.903 | 0.985 | 0.939 | 0.044 |
| 1x | 15 | 0.006 | 1.672 | 1.294 | 0.532 | 0.399 |
| 2x | 1 | 0.004 | 2.158 | 2.010 | 1.975 | 0.043 |
| 2x | 15 | 0.006 | 2.464 | 1.550 | 0.749 | 0.421 |

CPU seconds can exceed elapsed seconds through multiple process threads. Full
workload wall medians were 1.04–1.07 s except fifteen panes at 2x (1.225 s).
Coalescing under load means lower CPU or fewer paints does not imply a faster
renderer. One-row and full-screen figures are different workloads.

At 1x, increasing one to fifteen terminals raises full-output viewer CPU 1.31x
and all-hidden CPU 9.07x. At 2x those ratios are 0.77x and 9.79x; the former
reflects scene geometry/coalescing, not free terminal scaling. Fifteen sources
are thumbnails, while the single source occupies a larger part of the scene.

At 2x, fifteen full-output row caches retain 121.29 MiB versus 8.09 MiB for
one source (15x). Post-full-output viewer RSS is about 545 MiB versus 189 MiB
(2.88x); summed runtime RSS is about 118 MiB versus 7.8 MiB (15x, with shared
pages counted repeatedly). At 1x the corresponding viewer RSS is 245 versus
156 MiB (1.58x). Hidden phases retain those row caches. The 2x fifteen-pane
all-hidden viewer still spends 0.421 CPU s consuming/parsing/refreshing data;
zero paints does not mean zero client work.

These results establish a reproducible live data path and expose memory and
software-rendering costs. Native hardware results, particularly Mac Metal, and
physical-display quality still gate toolkit adoption. No frontend migration is
made by this benchmark.
