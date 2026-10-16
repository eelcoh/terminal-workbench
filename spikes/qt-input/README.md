# Qt Quick input probe

An isolated candidate-toolkit experiment. It does not open Workbench sessions,
start shells, or replace the viewer. It deliberately uses standard controls to
check the toolkit input and semantic APIs before building a custom terminal item.

Requirements: CMake 3.22+, C++17 compiler, Qt 6.8+ Quick and QuickControls2.
On this workstation install/build inside `build-machine`:

```sh
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland
just toolkit-probe
just toolkit-probe-test
```

On macOS install Qt development tools using your approved package source and
set `CMAKE_PREFIX_PATH` to that Qt installation if CMake cannot locate it. The
build creates an application bundle. The owner confirmed that the input-probe
API test passes on Mac; native IME and VoiceOver acceptance remain open.
Linux automated tests also need `xvfb-run`; interactive runs use the available
Qt platform backend (Wayland/X11). No download occurs in these recipes.

In the window, edit the workspace name, use an actual IME to compose/cancel/commit,
check candidate positioning, paste Unicode, select part of the existing name,
and Tab to the button. Check names, focus and activation with VoiceOver or an
AT-SPI inspector/screen reader. The button only increments a local counter.
The multilingual sample is for inspection, not proof of correct shaping.

The automated check injects Qt input-method events and queries accessibility
interfaces: preedit is not committed, cancel discards it, commit inserts once,
caret geometry moves, editable/button roles and names exist, text is readable,
and the advertised press action invokes the button. It does not test the native
IME service or OS accessibility bridge. See [readiness audit](../../docs/toolkit-readiness.md).

The test captures `.local/artifacts/toolkit-probe/qt-input.png` for visual inspection.

## Existing-runtime terminal host

```sh
just toolkit-terminal
just toolkit-terminal-test
```

Both recipes prepare the existing pinned VT/runtime first. The first opens a
**disposable shell** using the bundled Fira Code Regular at 18 logical pixels.
Closing it stops that temporary shell. It never loads your workspace catalog.
The automated test is Linux-only (Xvfb, xdotool and xclip); `toolkit-terminal`
also has a Mac app-bundle launch path: the owner confirmed launch and basic
command input. Other Mac interactions and display acceptance remain open.

The host uses Qt QPainter text on a QQuickPaintedItem, rather than Raylib font
textures. It restores a pinned Ghostty binary snapshot, then applies live output
to a client presentation replica. The existing Rust process owns the PTY, VT and
resize authority. The client never creates a second PTY or writes catalog state.
The bundled font and OFL notice are embedded as resources; system font fallback
handles missing glyphs.

Copy/Paste buttons work on both targets. Linux shortcuts are Ctrl-Shift-C/V;
Mac uses the standard Command-C/V. Ordinary Ctrl-C goes through Ghostty to the
shell. Drag selects, the wheel scrolls Ghostty history, and typing returns live.
The probe supports basic composition/cancel/commit and provides a caret rectangle.
It rejects surrounding-text replacement requests rather than editing shell-owned
input. Native IME service behavior, composition attribute/caret details, physical
keyboard layouts and negotiated Kitty composition remain unverified.

The Linux test checks snapshot/Unicode text and live output, exact ASCII/Enter/
Ctrl-C/Tab/arrow input, injected IME commit without preedit leakage, native pointer
selection and clipboard/bracketed-paste round trips, scrollback, PTY resize,
second-owner rejection, and same-reader-PID reopen. Artifacts are under
`.local/artifacts/qt-terminal/`; reports are opt-in and contain only disposable
test data. They are not enabled by the interactive recipe.

For developer-controlled attachment to a disposable existing runtime, run the
built `workbench-terminal-probe --control /path/to/session.ctl`. It refuses an
occupied session and closing this window merely detaches. There is no takeover
button. On Mac the executable is inside `workbench-terminal-probe.app/Contents/MacOS/`.

This is a one-terminal compatibility proof, not production rendering. It draws
complete graphemes per cell but does not shape across cells. Full terminal screen-reader usability, app mouse reporting, images, full query callbacks, selection
word/autoscroll tracking, multi-pane performance and display acceptance remain
open. QPainter's image-to-texture upload also needs profiling before adoption.


## Native Wayland and scale transitions

Linux test dependencies: Niri, Xvfb, xdotool, wl-clipboard, a C compiler,
wayland-scanner, and Wayland/xkbcommon development packages. These are the same
private-compositor tools used by the existing viewer tests. Run:

```sh
just toolkit-terminal-wayland-test
```

This starts private Xvfb/Niri instances without importing anything into your
session. Qt receives `QT_QPA_PLATFORM=wayland` and no `DISPLAY`; the opt-in report
must confirm the Wayland backend. A virtual Wayland keyboard supplies text and
Ctrl/Tab/arrow input. Pointer events enter through the nested compositor.

The test checks composition/cancel/commit injection and live output, transitions
through 1×, 1.25×, 1.5×, 2× and back, captured backing pixel dimensions, stable
logical window/grid, fresh forward/reverse/Unicode selection and clipboard/paste
at each scale, scrollback/live return, compositor PTY resize, and reopening at
1.5× onto the same process. Reports/screenshots are under
`.local/artifacts/qt-wayland-terminal/`. Composition injection does not establish
native input-method protocol compatibility or real candidate-window placement.
The no-CSD floating test window isolates client coordinates; decorated desktop
windows and physical mixed-display/GPU behavior remain separate gates.


## Terminal accessibility proof

```sh
just toolkit-terminal-accessibility-test
```

This Linux check needs `dbus-run-session`, AT-SPI2 and Python GObject bindings
(`python3-gobject` in build-machine), plus the existing Xvfb/xclip tools. It uses
private session/accessibility buses and a temporary runtime directory. Unrelated
portal activation is disabled only within this test environment; ordinary probe
launches keep the normal desktop services.

The custom terminal exposes a Terminal role, stable name, focused/busy/read-only
state, visible text, caret position, character geometry, selection and focus
action. Text ranges use Qt's UTF-16 offsets, mapping combining marks and surrogate
pairs to the corresponding terminal cell. Wide-character spacer cells do not
introduce fake spaces. Trailing blank cells/rows are omitted except where the
caret or selection needs them. Accessibility selection uses the same Ghostty
selection as mouse selection and Copy. Shell cursor movement and document
editing are not offered through this output-text interface.

The test checks Qt interfaces against a real PTY, then independently traverses
the AT-SPI tree, reads Unicode text and a line/emoji range, selects/removes a
range, focuses the terminal,
and receives a live-output text-change event. Existing exact-byte keyboard,
clipboard, resize and same-process checks then continue, detecting unintended
PTY input from accessibility actions. Artifacts include
`.local/artifacts/qt-terminal/atspi.json` and `run.log`.

Accessibility text is built lazily from the client grid. When accessibility is
active, visible terminal changes produce coalesced minimal text/caret/selection
notifications; the draw method only paints. This prototype exposes the viewport,
not a separate full-history document. It is not an Orca or VoiceOver usability
sign-off; speech volume, history navigation and native Mac bridging remain open.
On Mac use `just toolkit-terminal` and inspect the Terminal object with
Accessibility Inspector/VoiceOver. Native revalidation of this adapter is pending.


## Real IME service check (Linux only)

```sh
just toolkit-terminal-ime-test
```

Linux dependencies: IBus, IBus-Anthy, Python GObject/IBus bindings, Xvfb and
xdotool; the Wayland pass also needs the existing Niri and Wayland/xkbcommon test
tools. In build-machine the additional packages are `ibus ibus-anthy`.

The recipe runs two isolated sessions. First, X11 isolates Qt's input-method
adapter and real engine behavior from compositor behavior. Second, a native Qt
Wayland window runs in private nested Niri with `DISPLAY` removed. Both explicitly
select Qt's IBus input context (`QT_IM_MODULE=ibus`) and connect to the private
IBus service through D-Bus. This does not exercise compositor-managed Wayland
`text-input`/`input-method` protocols.

Keyboard events produce `にほん` preedit in Anthy, Escape cancels without PTY
input, Space converts to `日本`, and Enter commits exactly once. A second commit
cycle plus subsequent Enter/Ctrl-C detects stale composition and duplicated or
eaten terminal input. No QInputMethodEvent is injected by this recipe; those
events come from Qt's real IBus plugin. The opt-in probe report includes the
current preedit, composition caret, terminal focus and Copy button position solely
to make these checks observable. Real pointer clicks move focus to Copy twice:
pending composition clears, typing on the button sends no PTY bytes, and clicking
the terminal restores fresh composition. A second Qt window is refused runtime
ownership; activating and closing it checks window-level cancellation and return
to the original attachment. On Wayland the private Niri window floats at a fixed
position; pointer events pass through Niri's nested X11 backend to native Wayland
Qt, while its keyboard uses the private virtual Wayland seat.

Every run uses private session bus, IBus address, XDG runtime/configuration/cache/
data paths and in-memory GSettings. The engine is selected through IBus's API
without the CLI's keyboard-layout side effect. Panels are disabled, so candidate
popup placement is not tested. Artifacts are under
`.local/artifacts/qt-ime/{xcb,wayland}/`, including preedit screenshots and raw PTY
bytes. Composition segment/cursor rendering also has API/pixel coverage in the
terminal smoke suite. Native Mac IME, compositor-managed Wayland IME, candidate
popup placement and focus transitions between two editable terminal clients
remain acceptance gates.

## Native Mac IME check

`just toolkit-terminal-ime-test` uses Linux IBus/Anthy and private X11/Wayland
desktops. It deliberately exits with an unsupported-platform error on Mac; it
does not run a Mac IME test.

On Mac run `just toolkit-terminal` to open a disposable native Qt terminal, then
run this command inside it:

```sh
python3 -c 'print(repr(input("Type here: ")))'
```

First add Japanese – Romaji in System Settings → Keyboard → Text Input → Edit →
+ → Japanese → Japanese – Romaji → Add. Return to the terminal and use the
macOS menu-bar input menu (often ABC) to select Hiragana / あ. The Japanese –
Romaji input source uses the regular keyboard; Hiragana is the composition mode.
See Apple's [setup](https://support.apple.com/en-gb/guide/japanese-input-method/jpim10267/mac)
and [switching](https://support.apple.com/en-gb/guide/japanese-input-method/jpimf6ffb247/mac) instructions.

Type `nihonn` and check the
composition appears at the caret; Escape should cancel it. Repeat, press Space (several times if needed) to
show conversion candidates, convert to `日本`, and select/confirm the candidate. Press Return as needed to
submit the line. Python should print `'日本'` exactly once, with no cancelled
text or extra characters. Check that the candidate panel stays near the caret.

Repeat the command and move focus to Copy while composition is pending, then
click back into the terminal. Check for stale composition or unexpected input;
repeat with another app/window. Record what happens rather than assuming the
Linux engine's cancellation policy applies to macOS. This is manual acceptance,
not automated exact-byte verification. The earlier `toolkit-probe-test` uses
injected composition events and does not prove native Mac IME integration.

## Text shaping and font fallback (Mac and Linux)

```sh
just toolkit-text-test
```

This recipe generates artifacts and exits; it does not open a terminal window.
Open `.local/artifacts/qt-text/comparison-1x.png` for the comparison sheet and
`report.json` for actual font names and glyph/UTF-16/cell mappings. Images at
1.25x, 1.5x and 2x are saved alongside it. On Mac, for example:

```sh
open .local/artifacts/qt-text/comparison-1x.png
```

The left column draws each actual pinned-Ghostty cell separately, using the
terminal probe's bundled Fira Code at 18 px and system font fallback. The right
column uses Qt's full-text layout of the same source string with natural Unicode
direction and advances. It covers accents, Arabic, Devanagari, CJK, emoji/ZWJ/
flags, programming ligatures, box drawing and a grapheme with 25 combining marks.
All VT source text must round-trip, including supplementary characters and long
graphemes. Tests verify glyph metadata and the scaled PNG dimensions.

A successful command means the evidence was generated and validated. It does
not mean every font is present or that terminal shaping is correct. Missing
glyph counts and actual fallbacks are reported per specimen; glyph differences
compare font/style/glyph multisets, not a semantic correctness oracle. No font
names or platform-specific glyph IDs are frozen as golden results.

The full-text reference is not a replacement terminal renderer. It may combine
or reorder text across terminal cells and use advances that do not fit the
fixed grid. Correct cell ownership, cursor/selection geometry, terminal ordering,
style boundaries and clipping need a separate integration design. The export
scale exercise uses QImage rasterization, not physical GPU/mixed-DPI validation.
No runtime, PTY, saved session or Workbench appearance setting is opened.

## First integration: shaping within the terminal grid

The Qt terminal probe now uses cached row images produced from shaped text runs.
Runs end at style, selection and unwritten-cell boundaries. Source-range glyph
queries and Unicode grapheme boundaries map joined glyphs back onto Ghostty's
original columns, including wide-cell tails. The terminal owns cursor positions,
selection and copied text throughout. Glyphs are not stretched to fill the grid.

`just toolkit-text-test` now also writes `grid-1x.png`, `grid-1.25x.png`,
`grid-1.5x.png` and `grid-2x.png`, using the same rendering function as the
interactive Qt host. The original `comparison-*.png` sheets retain the old
per-cell baseline. Each sheet's glyph-difference footer describes that baseline,
not an independent shaping verdict on the integrated image. The report adds
`grid` glyph placements with first/inclusive and last/exclusive source columns.
Raw Qt UTF-16 glyph indexes are diagnostic only: fallback runs may return local
indexes, so the terminal integration uses range queries instead.

The LTR specimens must retain the full-text reference's font/glyph multiset,
including repeated combining glyphs. Tests check wide-cell ownership, multi-cell
emoji, style boundaries, supplementary RTL characters, unchanged-row retention
and selection/scale invalidation. Native GUI checks select/copy/paste a shaped
`->` and verify new raster resolution at every Wayland scale transition.

This first integration retains the old per-cell policy for runs containing RTL
text or bidi controls; it does not implement paragraph reordering or correct RTL
joining. Emoji whose source spans several Ghostty cells keep those cell widths,
so visible spacing can remain large. Fixed-grid Indic spacing, tall fallback
clipping and box-drawing seams still need review.

Rows are shaped and rasterized on the GUI thread only when their contents, style
or scale change. Paint draws cached images, keeping QRawFont glyphs on their
creating thread. Hidden/minimized sources skip the new rasterization work and
refresh on reveal. One extra CPU bitmap per visible terminal row is retained;
memory, CPU/GPU load and the one/fifteen-pane gate remain to be measured before
frontend adoption. Mac compilation and live-terminal visual acceptance of this
integration remain pending.

## CPU raster/cache performance (Mac and Linux)

```sh
just toolkit-perf
```

The recipe uses a separate Release build under `.local/build/qt-performance`
and prints a table before exiting. It does not open terminal windows or touch
PTYs/saved sessions. Results, raw trial reports and logs are written to
`.local/artifacts/qt-performance/`; share that directory for review.

Each of 1 and 15 panes retains an independent fixed 80×24 grid, at raster scales
1x and 2x, with ASCII and mixed-Unicode fixtures extracted from actual Ghostty
VT state. This is fixed geometry per pane, so the 15-pane case paints fifteen
times the pixel area; it is not fifteen smaller tiles in one fixed-size window.
The Unicode profile has a different glyph/occupied-cell mix, so compare pane
counts within a profile, not language speed across profiles.

Three fresh child processes run each case. Within each process font/cache
warm-up precedes five timed scenarios: unchanged rows; one changed row per pane;
all rows changed; all panes changing with only one visible; and all panes
hidden. Samples target 300 ms with at least three frames and a 20,000-frame cap
for cheap hidden loops. Medians/ranges retain the raw observations rather than
enforcing wall-clock CI thresholds. The final reveal forces a distinct full
generation and checks every row catches up.

The live terminal host and benchmark share `TerminalTextSurface`, including its
visibility early exit. Deterministic assertions require zero new rasterizations
for retained/hidden sources, exactly the expected dirty rows for output, and
correct reveal catch-up. Hidden timings still include synthetic grid mutation
and the visibility calls; they do not measure real background PTY parsing.

Reports separate per-scenario CPU/wall time, row-image bytes, synthetic CPU
composite-target bytes and each child's peak RSS/process CPU. Peak RSS is obtained
from `wait4` for the exact child and normalized to bytes for Linux/macOS. The
composite targets are CPU image buffers, not GPU allocations. Cache memory
remains retained when sources hide. Timing startup and cold font loading are
excluded from the frame samples but included in process CPU/RSS.

This measures row shaping/raster/cache checks and CPU image compositing only.
It excludes Qt Quick frame scheduling/texture upload, GPU/VRAM, live per-cell
background/cursor/chrome paint, VT parsing, socket/snapshot work and physical
monitor behavior. An unchanged real terminal need not paint at all; the retained
scenario deliberately forces compositing to measure its cost. Native GPU and
full live 1/15-pane acceptance remain separate toolkit gates.

## Batched glyph drawing and guarded ASCII mapping

The Qt row renderer batches consecutive glyphs using the same raw font into one
draw call, preserving draw order, source positions and style boundaries. Plain
printable ASCII with one narrow scalar per cell can also use a single Qt glyph
query when Qt reports one primary-font run, the complete source string, and
valid monotone source indexes beginning at zero. Font fallback, non-ASCII and
unaccepted mappings continue through per-source-range queries.

`just toolkit-text-test` compares optimized raster pixels against the original
source-range/per-glyph grid renderer for every specimen and all four scales.
Additional ASCII cases protect glyph positions/ownership and normal, bold,
italic, selection/color/underline boundaries. The offline `grid` report exposes
`bulk_ascii` on placements so a native run can show whether the guard accepted
the fast path. The reference renderer is an internal test mode, not a setting.

`just toolkit-perf` uses the same workload as the earlier CPU baseline. Compare
timings within a profile and scale; the optimization does not reduce row-image
bytes, change RTL policy or establish GPU/physical-display performance. Native
Mac pixel equivalence and timings still need to be run for this change.


## Retaining only the written row extent

The live cache allocates foreground images only through the last written cell
or wide-character spacer. Unwritten trailing cells remain in the source grid;
backgrounds, selection, cursor and PTY geometry still cover the full terminal.
Written spaces can carry underlines and are retained. A completely empty row
keeps a one-pixel-wide transparent image with the current device-pixel ratio.
The image origin stays at column zero, preserving fractional-scale raster phase.
No pixel scanning, image-copy cropping or extra work is added during paint.

The full-width source-range renderer remains the pixel oracle. Text tests
composite each cached image onto a full-width transparent canvas and require
exact equality across the complete text corpus and scales, plus empty, growing,
shrinking, full-width, underlined-space, styled and wide-tail cases.
`grid-*.png` sheets now paint the actual retained images.

Run `just toolkit-text-test` and `just toolkit-perf` on each platform. The latter
now prints the source checkout and tracked-change status and includes them in
its report, making pasted benchmark logs attributable. These identify the
runner checkout; timings still measure only the isolated CPU path. Savings
depend on unwritten columns; a full row retains its original allocation.
Composite targets, Qt Quick textures and VT storage are unaffected.


## Live Qt Quick scene measurement

`just toolkit-scene-perf` builds a separate Release executable and opens a series
of synthetic-terminal windows on the current Mac/Linux display. Keep them
visible and unobscured; do not resize them or move them between displays during
the run. It runs three fresh processes for each 1/15-pane ASCII/Unicode case.
Native DPR/backend are recorded, not forced; repeat after moving to another
monitor to compare that display. Closing a benchmark window fails the run.

The fixed 1120×680 window contains one full-size terminal, or fifteen scaled
thumbnails. Each source remains 80×24 cells with the same unscaled painted-item
size and window DPR, so fifteen panes multiply source pixels while fitting into
one viewport. This is a controlled scene workload, not the product overview UI.
The executable shares the real host's full-grid background/foreground paint
function and the CPU benchmark's Ghostty-extracted fixture. It adds a fixed
cursor outline, with no PTYs, saved sessions, sockets, IME or terminal input.

Each process warms up three frames per scenario, then measures at least 300 ms
and three frames:

- `cached-scene`: request window frames with cached item textures, no item repaint.
- `repaint`: repaint visible items without changing cached glyph rows.
- `one-row`: change one source row per visible item, repaint a bounded row band.
- `full`: change every row and repaint all items.
- `one-visible`: update all sources while hiding fourteen items; only the visible
  item's rows and image paint. Reveal catches up hidden rows outside timing.

One generation is in flight at a time. Render-thread synchronization/swap tokens
prevent old frame notifications from acknowledging a newer update. Exact paint,
dirty-row and reveal counts gate success. Frame timings run from GUI update
request to Qt's `frameSwapped` notification, which means queued for presentation,
not GPU completion. Vsync/compositor/event-loop pacing is included. GUI update
and summed item-paint times are reported separately, as is process CPU time.
These do not isolate upload cost, GPU execution, refresh-rate FPS or input latency.

Artifacts are in `.local/artifacts/qt-scene-performance/`: aggregate JSON, raw
reports, startup backend logs and a rendered scene capture per trial. Reports
record native DPR, API, geometry, source checkout, environment and exact-child
peak RSS. `estimated_rgba_item_bytes` is a model of one full-size RGBA buffer per
item; it is not measured VRAM and does not count additional copies/driver memory.

`just toolkit-scene-perf-test` is Linux-only: it runs the same matrix on private
Xvfb at explicit 1x/2x using software OpenGL, asserting that the RHI OpenGL path
is active. Results go to `.local/artifacts/qt-scene-test/`. Software OpenGL
exercises scene/texture plumbing but does not establish hardware GPU performance.


## Opaque background spans

The shared live/benchmark grid painter combines adjacent cells with the same
opaque background into one row span. An ordinary uniform 80×24 grid uses 24
grid-background fills instead of 1,920; the full pane backdrop and cursor remain
separate. Mixed theme/selection colors end a span, and the foreground row-image
order and complete source geometry are preserved.

The painter retains per-cell fills for translucent colors, painter opacity,
antialiased rectangles, non-default composition, rotation or shear, where edge
coverage could change. `just toolkit-text-test` compares against the original
per-cell background compositor at all four scales, two cell widths, fractional
origins, multiple colors, alpha, opacity, antialiasing, composition, transforms
and clipping. `just toolkit-scene-perf` measures the effect on actual painted
items; cached scene redraws should continue to perform zero item painting.


## Changed-row invalidation

The shared row cache returns presentation damage: first/last changed rows or a
full repaint when the cache is initialized, resized, rescaled or given new font
metrics. Hidden updates leave cache/damage untouched; reveal catches up later.
The Qt host requests full-width row bands with one logical pixel of padding,
so shorter lines erase old suffixes and fractional raster/stroke edges survive.
Cursor changes include both old and new cursor rectangles. Background changes
and active preedit use full bounds. Resize, DPR and composition updates request
explicit full bounds so later partial updates cannot narrow pending full damage.
Empty damage skips `update` entirely; an empty Qt update rectangle means full.

Painting honors Qt's clip and skips drawing rows outside it. A one-row update
in the scene fixture draws three row bands (the changed row and clipped neighbor
edges) instead of 24; `painted_grid_rows` records this independently of raster
counts. The benchmark's `repaint` and `full` cases continue requesting full item
paints. Hidden sources still do no raster or image paint work.

`just toolkit-text-test` retains an image across text/erase/selection/cursor and
coalesced-update transitions and requires exact equality with fresh full paints
at 1x/1.25x/1.5x/2x. `just toolkit-scene-perf` additionally compares the actual
partial scene capture with a forced full repaint of identical source state.
These readbacks run outside timing and the report requires
`partial_matches_full: true`. Native Mac measurements need rerunning for this
change. Full-size backing images/textures remain; no upload-area or VRAM saving
is assumed from CPU clipping.
