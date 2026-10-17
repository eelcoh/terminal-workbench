# Production toolkit readiness

Status: 2026-10-17. Keep the approved minimal chrome and Overview interaction.
The Raylib viewer remains the working experiment; it is not ready for production
adoption. Qt Quick is the first candidate to test, not a selected replacement.

## Source audit of the current viewer

The pinned Raylib 5.5 tree uses modified GLFW 3.4. Review of its Wayland registry
bindings (`src/external/glfw/src/wl_init.c`) found no text-input/input-method
protocol integration. The Workbench `platform_keyboard.h` bridge carries key
and committed-character events, without preedit or surrounding-text queries.
The passing XKB dead-key test therefore does not establish full IME support.

The Cocoa backend (`src/external/glfw/src/cocoa_window.m`) stores marked text,
but `firstRectForCharacterRange:actualRange:` returns view-frame coordinates
with zero size rather than a terminal caret rectangle. Workbench does not project
that composition into its text fields or terminal surface. Native candidate
placement and composition still need direct Mac acceptance.

Workbench draws controls as text and rectangles. No accessibility adapter/tree
was found in `spikes/ghostling`, or the pinned Cocoa backend. Keyboard navigation
alone does not expose control roles, values, focus and actions to screen readers.

`spikes/ghostling/terminal_font.h` draws codepoints separately, skipping variation
selectors and joiners. The viewer truncates graphemes to sixteen codepoints.
There is no shaping-run model; extra font coverage is not complex-script shaping.
Toolkit text controls can solve chrome typography without automatically solving
custom terminal rendering. Ghostty's full embedding header also describes an
application-oriented surface; it is not a ready-made portable terminal widget.

Pinned sources are recorded by the viewer build scripts: Ghostling
`63842bf8e5e481160f81d348da9ff6fd27986798` and Ghostty
`f64f4aca2c29b554d111b36c3d946a9bddd159ff`. Generated source under `.local/`
is not the maintained patch location; changes belong in build patch scripts.

## Candidate assessment

| Candidate | Reason to investigate | Integration still to prove |
| --- | --- | --- |
| Qt Quick | Standard text controls, explicit composition/caret APIs and semantic controls; flexible animated chrome on both targets. | Custom terminal rendering, OS accessibility, packaging and native Mac quality. |
| Slint | Rust integration and shared declarative UI; multiple desktop rendering backends. | Renderer/backend choice, terminal integration, native composition and accessibility behavior on each target. |
| GTK4 + AppKit, or shared GTK4 | Native input/accessibility infrastructure and a Linux GL surface option. | Maintaining one or two clients, matching visual interactions and terminal rendering on Mac. |

Qt separates [preedit and commit events](https://doc.qt.io/qt-6/qinputmethodevent.html)
and supports [accessible Quick controls](https://doc.qt.io/qt-6/accessible-qtquick.html).
Its [QQuickRhiItem](https://doc.qt.io/qt-6/qquickrhiitem.html) offers custom rendering
through QRhi; it does not make an existing OpenGL texture portable to Metal.
Slint's [backend/renderer choices](https://docs.slint.dev/latest/docs/slint/guide/backends-and-renderers/backends_and_renderers/)
have different text and integration constraints. GTK exposes
[accessibility](https://docs.gtk.org/gtk4/section-accessibility.html) and a
[GLArea](https://docs.gtk.org/gtk4/class.GLArea.html).

Distribution must use the selected modules' actual license terms and preserve
notices. Workbench's MIT license does not change toolkit dependency licensing.
See [Qt licensing](https://doc.qt.io/qt-6/licensing.html) and
[Slint licensing](https://github.com/slint-ui/slint/blob/master/LICENSE.md).

## First proof: isolated Qt input and semantic controls

`just toolkit-probe` builds a separate window with a workspace-name field,
button, and multilingual text sample. It never opens runtime/catalog state.
`just toolkit-probe-test` checks composition/cancel/commit, caret geometry,
accessible roles/names/text and activation through Qt APIs. Linux uses private
Xvfb and software rendering. The probe passed with Qt 6.11.2 in build-machine
on 2026-10-15; full `just check` also passed. The captured window was inspected
for layout only. Native Mac execution is unverified. This proves API plumbing
only, not native IME,
AT-SPI/VoiceOver end-to-end, shaping correctness, or production typography.

## Gates before selecting a production frontend

1. Use real Japanese/Chinese composition and cancellation on native Wayland and
   macOS. Position candidates at the actual caret; send only committed text to
   the runtime. Preserve application shortcuts, selection and clipboard behavior.
2. Verify semantic control names, roles, values, focus, selection and actions with
   AT-SPI and VoiceOver. Define readable terminal content without flooding speech.
3. Host one existing runtime terminal with Ghostty state/encoding, then prove
   resize, selection, scrollback, Unicode paste, ownership transfer and same-PID
   detach/reattach. Avoid a second authoritative emulator or child-process owner.
4. Choose and inspect terminal shaping/font rasterization with combining marks,
   joined scripts and fallback fonts. Compare native Mac and external-display
   sharpness, fractional scaling, unplug recovery and physical trackpad input.
5. Reproduce the approved chrome/Overview slice. Measure idle and output load at
   one and fifteen terminals, rendering retained/hidden sources only as needed.
6. Demonstrate packaging on both targets and review the result with the owner
   before replacing the experimental viewer or selecting the toolkit.


## Second proof: an existing runtime terminal hosted by Qt

`just toolkit-terminal` opens one disposable runtime-owned shell. It restores
Ghostty's pinned snapshot and applies live output to a presentation replica.
Qt QPainter draws the cell graphemes using bundled Fira Code; Raylib glyph
textures are absent from this path. This makes it a useful future comparison
for Mac sharpness, without claiming a rendering fix.

`just toolkit-terminal-test` exercises the real runtime and a private Linux Xvfb
window. It covers snapshot restoration, Unicode and styled text presentation,
live output, exact keyboard bytes, injected composition/cancel/commit, native
selection/copy and bracketed paste, scrollback, PTY resize, occupied-session
rejection and same-process reopen. The final suite passed with Qt 6.11.2 on
Linux on 2026-10-15, alongside the original input probe and full `just check`. See the [probe instructions](../spikes/qt-input/README.md).
The test exposed and corrected Qt Tab traversal stealing shell input and Ctrl
text being interpreted twice by the terminal encoder.

Painting uses immutable cell snapshots prepared on the GUI thread, coalescing
terminal changes with a single-shot 16 ms refresh rather than idle repainting.
The transport bounds frame sizes, snapshot bytes and queued input. Snapshot
decode is still synchronous and each changed screen is copied in full; this
single-terminal experiment does not establish fifteen-pane scaling. Qt documents
[QQuickPaintedItem's image-to-texture path and performance tradeoffs](https://doc.qt.io/qt-6/qquickpainteditem.html).

Remaining gates include real platform IME, terminal accessibility, cross-cell
shaping, app mouse/graphics support, protocol coverage, multi-pane performance
and native Mac interaction/display acceptance. Later Wayland and basic Mac
launch/input results are recorded below. The owner cannot compare
on the external Mac display for four days (approximately 2026-10-19); sharper
text, scale changes and unplug recovery stay explicitly pending until then.


## Native Mac input-probe result (2026-10-15)

The owner supplied native Mac output from `just toolkit-probe-test`. CMake
configured and the binary built successfully; the composition/cancel/commit,
caret geometry and accessible roles/text/action checks passed. Optional Vulkan
header discovery did not prevent this build or test. Qt reported missing OpenType
support for script 11 in `.AppleSystemUIFont` and `.AppleIndicFont`; multilingual
sample shaping and font fallback still need visual review. This does not prove
real IME service integration or VoiceOver behavior.

This log is from the standard-control input probe, not `just toolkit-terminal`.
The later terminal-host launch/input result is recorded below; this input-probe
log does not establish terminal-host behavior or external-display quality.


## Linux portal warning during interactive launch

The owner reported `qt.qpa.services: Failed to register with host portal` with
`App info not found for 'terminal-workbench-terminal-probe'` from
`just toolkit-terminal`. The probe advertises that desktop ID without installing
a corresponding desktop entry. The [portal Registry contract](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.host.portal.Registry.html)
requires an ID matching a desktop-file basename. In
[Qt 6.11's registration code](https://raw.githubusercontent.com/qt/qtbase/6.11/src/gui/platform/unix/qdesktopunixservices.cpp),
this asynchronous failure is logged through `qCWarning`, not an application
exit. The message alone therefore does not establish a launch failure. A proper
host-visible desktop entry remains a packaging task, including Distrobox launch
paths. Do not disable the session bus or hide all Qt warnings to work around it.

The Mac search-log format warnings were corrected using `PRIu64` for the VT's
64-bit viewport offset. Linux viewer build and full `just check` passed after the
correction; native Mac compilation of this small logging change is pending.


## Native Qt Wayland and scale validation

On 2026-10-15 the native Wayland suite passed with Qt 6.11.2, nested Niri
and software rendering. The X11 terminal-host suite, standard-control input
probe and full `just check` also passed. Captures were visually inspected for
layout; they are not evidence of physical-display sharpness.

`just toolkit-terminal-wayland-test` exercises a disposable Qt terminal host in
private nested Niri, with the Qt backend reported as `wayland` and `DISPLAY`
removed. It checks exact keyboard and Unicode bracketed-paste bytes, injected
composition/cancel/commit, live output, pointer selection in both directions,
Unicode clipboard round trips, scrollback and return to live, compositor-driven
PTY resize, and reopening at fractional density with the same reader process.

Scale transitions 1 → 1.25 → 1.5 → 2 → 1 preserved an 800×500 logical window and
16×65 PTY grid. Qt window captures measured 800×500, 1000×625, 1200×750,
1600×1000 and 800×500 pixels respectively. Selection/copy/paste was exercised
with a fresh clipboard sentinel at every scale, plus fresh-window 1.5×. A
single-shot screenshot request and backing-scale fields exist only in opt-in
test reporting; normal interactive launches do not perform these captures.

The owner also confirmed that the Qt terminal host builds, opens and accepts
commands on Mac. Clipboard, selection, scrolling and resizing there still need
manual confirmation. Real platform IME, terminal accessibility, cross-cell
shaping, physical GPU/mixed-display behavior and Mac external-screen sharpness
remain open. This evidence does not select Qt as the production toolkit.


## Terminal text accessibility proof

On 2026-10-15, Qt interface and real AT-SPI checks passed with Qt 6.11.2
and AT-SPI2 2.60.7 in build-machine. Native Wayland/scale regressions, the
standard-control input probe and full `just check` also passed.

The Qt terminal host now has a custom public-API accessibility adapter, distinct
from the standard-control input probe. It exposes a Terminal role and visible
output text with cell-aware geometry, caret/selection information and a focus
action. The output text is read-only; document editing and arbitrary shell cursor
movement are not synthesized. Unicode combining marks, wide cells and UTF-16
surrogate pairs map back to the rendered cells. Selection delegates to the same
Ghostty state used by mouse selection and Copy.

The adapter uses [QAccessibleTextInterface](https://doc.qt.io/qt-6/qaccessibletextinterface.html)
and [QAccessibleObject](https://doc.qt.io/qt-6/qaccessibleobject.html) without private
Qt headers. A lazy viewport snapshot avoids formatting text when accessibility
is unused; rendering remains pure. Active accessibility receives coalesced text,
caret and selection events, with unchanged prefix/suffix text excluded from
update notifications. No per-byte announcements or full-history scans are added.

`just toolkit-terminal-accessibility-test` checks role/tree links, readable
Unicode text, combining/wide/emoji geometry, selection/copy, focus changes and
absence of a document-editing interface. A second process traverses the real
Linux AT-SPI tree over private session/accessibility buses, reads the terminal,
navigates a line and an emoji range, changes selection/focus, and observes an event from new real PTY output. The
same runtime then passes the existing exact-byte/clipboard/resize/reopen checks.
The test isolates runtime paths and suppresses unrelated desktop portal startup
only inside the disposable harness.

Native Mac build/VoiceOver review of this adapter, Orca interaction, reading order,
speech policy during sustained output and full-history accessibility remain open.
The pinned Qt AT-SPI bridge exposes Qt UTF-16 text offsets; interoperability around
supplementary characters needs assistive-tool review before production adoption.
This completes a bounded API/bridge proof rather than the accessibility gate.


## Real IBus/Anthy composition on X11 and Wayland

On 2026-10-16 both passes succeeded with Qt 6.11.2, IBus 1.5.34 and
IBus-Anthy 1.5.18. Native Wayland/scale, terminal accessibility, standard-control
input and full `just check` regressions also passed. Preedit captures were
visually inspected; physical-display quality is still unverified.

The earlier injected-event checks exercise the custom item's API only.
`just toolkit-terminal-ime-test` adds keyboard → Qt input context → private IBus
service → real Anthy engine → composition/commit event → PTY verification.
No composition event is synthesized by the harness.

X11 is a useful baseline here: it isolates the real Qt/engine path before adding
a compositor. It is not a substitute for Wayland acceptance. The same recipe
therefore also runs a native Qt Wayland window in private nested Niri, verifies
its reported backend and removes `DISPLAY`. Both passes explicitly use
`QT_IM_MODULE=ibus`, connecting Qt directly to the private IBus service over D-Bus.
They do not cover compositor-managed Wayland `text-input`/`input-method` routing.
Qt's [IBus adapter source](https://raw.githubusercontent.com/qt/qtbase/6.11/src/plugins/platforminputcontexts/ibus/qibusplatforminputcontext.cpp)
shows this service bridge and its composition/commit delivery.

Observed behavior: `nihonn` composes `にほん`; Escape clears preedit with no PTY
bytes. Space converts it to `日本` without committing, and Enter sends exactly
one UTF-8 commit. A second cycle commits Hiragana without duplication. Enter and
Ctrl-C afterward still produce their ordinary terminal bytes. Screenshots show
preedit in the terminal; raw-reader output distinguishes composition from input.

The private daemon, bus and XDG paths plus in-memory GSettings isolate all engine
settings and dictionary data from the user's desktop. Direct engine selection
uses IBus's API; `ibus engine` also tries to change keyboard layout and can return
failure after successful selection. Panels are disabled. Candidate popup
placement, compositor protocol routing and native Mac composition remain open.
Later slices below cover preedit styling/caret attributes and bounded control/
window focus transitions. The main Workbench
viewer is unchanged, and these checks do not settle the toolkit decision.

## Composition formatting and caret

The isolated terminal host now lays out preedit text with Qt's `QTextLayout`,
retaining the engine's segment foreground/background/underline formatting and
its UTF-16 cursor position, visibility and color. Its input-method rectangle
follows the composition cursor rather than staying at the shell cursor. Layout
is rebuilt on composition events; painting only draws the cached overlay. This
is composition shaping, not cross-cell shaping of terminal output.

The terminal smoke test checks formatted segment pixels, cursor visibility,
supplementary-character offsets, cancellation cleanup and the default end caret.
The real IBus/Anthy suite additionally moves Left/Right inside the preedit and
checks the reported caret moves without sending arrow bytes to the PTY.
Cancellation restores the terminal caret; commit-only checks still apply.
The Linux checks passed with Qt 6.11.2 on 2026-10-16, including real IBus/Anthy
on X11 and native Wayland, scale/clipboard/reopen coverage and AT-SPI regression
checks. The current Workbench frontend is unchanged.

This follows Qt's [input-method attribute contract](https://doc.qt.io/qt-6/qinputmethodevent.html)
and [text layout API](https://doc.qt.io/qt-6/qtextlayout.html). Native candidate
popup placement, compositor-managed Wayland input methods and Mac IME acceptance
remain open; control/window focus checks are recorded below.

On 2026-10-16 the owner confirmed native Japanese composition and the candidate
popup in the Mac Qt terminal probe after adding Japanese – Romaji and selecting
Hiragana. This is a manual composition/popup check; cancellation, exact-once
commit, focus transitions, VoiceOver and external-screen sharpness remain
unconfirmed on Mac. The earlier `toolkit-terminal-ime-test` invocation only hit
the Linux-only guard. See the [native procedure](../spikes/qt-input/README.md#native-mac-ime-check).

## IME focus transitions

`just toolkit-terminal-ime-test` now exercises real pointer focus changes with
IBus/Anthy on both X11 and native Wayland. Twice in succession it composes pending
Hiragana, clicks the existing Copy button, verifies preedit cancellation without
committing to the PTY, types on the button without terminal input, and clicks back
into the terminal. Fresh composition still converts and commits exactly once.

A second real Qt window attempts to attach to the occupied runtime and is
explicitly verified as refused. Activating that window cancels pending preedit
without committing; after closing it the original attachment and shell reader
PID remain available, with no stale composition on return. The probe reports
focus and control geometry only when the disposable test report is requested.
The Wayland pointer route uses the private nested compositor's X11 host; the
Qt client still runs on native Wayland with `DISPLAY` unset. This is not a
physical-device test.

These checks pass with the existing Qt integration; no application-wide reset
policy was added. Qt documents automatic reset when the focused editor changes
in its [input-method API](https://doc.qt.io/qt-6/qinputmethod.html). The bounded
Anthy results do not establish other engines' behavior, transfers between two
editable terminals, native Mac focus/IME, compositor-managed input-method
protocols or candidate panel placement.

## Text shaping and fallback evidence

`just toolkit-text-test` is an offline Mac/Linux evidence generator, exporting
comparison sheets at 1x/1.25x/1.5x/2x and JSON font/glyph/UTF-16/cell mappings.
It uses the pinned Ghostty VT to obtain real terminal cells, then compares the
current Qt host's per-cell drawing strategy against Qt full-text layout with
the same bundled Fira Code 18 px and system fallbacks. No PTY or user session
is involved. Glyph inspection runs once per fixed specimen, outside terminal
refresh/paint loops; images are exported once per scale.

On Linux/Qt 6.11.2 on 2026-10-16, the comparison reproduced different glyph
identities for programming ligatures, Arabic, Devanagari and emoji. No zero-index
missing glyphs were reported for any specimen with this machine's fonts. The
fallbacks included DejaVu Sans Mono for Arabic, Noto Sans Devanagari, Noto Color
Emoji, Maple Mono NF CN and Noto Sans CJK JP. These names are observed local
inventory, not portable dependencies or recommended defaults. Accents and CJK
had the same glyph identities in both paths, which does not establish identical
placement or rasterization. The 25-mark grapheme remained intact in one VT cell.

Some emoji sequences already span multiple VT cells in the specimen. The
report retains each cell's source text and column so that VT segmentation can
be distinguished from cell-isolated drawing. Full-text shaping visually combines
some sequences that separate cell drawing leaves apart. This is evidence of a
remaining gap, not a fix or an acceptance result.

The PNGs were inspected. Arabic joining/direction, Indic shaping and programming
ligatures visibly differ. A full-text layout is not safe to drop directly into
the terminal: its natural direction and variable advances can move text away
from Ghostty-owned cells. A candidate integration must map shaped clusters back
to their source cell spans, preserve terminal order, wide-cell ownership,
selection/cursor positions and style boundaries, then check fallback clipping
and baseline alignment. Raster exports do not settle external-screen sharpness.

Qt's [layout API](https://doc.qt.io/qt-6/qtextlayout.html) and
[glyph-run API](https://doc.qt.io/qt-6/qglyphrun.html) provide the diagnostic
font/glyph/source mappings. Glyph identity differences and missing-glyph counts
are diagnostic signals, not proof of language correctness. Cross-cell shaping,
native Mac terminal integration and the one/fifteen-pane performance gate
remain open; the offline Mac comparison is recorded below. See the [cross-platform procedure](../spikes/qt-input/README.md#text-shaping-and-font-fallback-mac-and-linux).

## Mac text comparison result

On 2026-10-16 the owner supplied a successful `just toolkit-text-test` log and
all four raster-scale images plus the report from native Cocoa/Qt 6.12.0.
The artifact validator passed when rerun against those supplied files; 1x and
2x comparison images were inspected. Bundled Fira Code was active at 18 px,
with the same 12 × 23 logical cell geometry as the Linux run.

The glyph-identity differences matched the Linux specimen categories: programming
ligatures, Arabic, Devanagari and emoji. Accents, CJK, box drawing and the long
grapheme had matching glyph identities across the two paths. The report contained
zero zero-index missing glyphs, but the cell-rendered emoji row visibly splits
sequences, including modifier/flag components. A zero count therefore does not
establish visually correct fallback, composition or placement.

The Mac reference used Courier New for Arabic, Kohinoor Devanagari, Hiragino Sans,
Apple SD Gothic Neo and Apple Color Emoji alongside Fira Code. These are observed
fallback choices, not newly bundled assets. Qt logged missing OpenType support
for Fira Code/script 11 while the report showed Devanagari fallback. Qt's
[font-database source](https://raw.githubusercontent.com/qt/qtbase/6.12/src/gui/text/qfontdatabase.cpp)
logs this at informational level when a font engine lacks support for a script.
The optional Vulkan-header discovery messages also did not stop configuration,
building or artifact validation in the supplied run.

This closes the offline Mac specimen-generation/font-inventory check. It does
not validate a shaped terminal renderer, language correctness, physical external
display sharpness, monitor changes or GPU behavior. The next shaping step must
preserve Ghostty's source-cell mapping and terminal interaction geometry while
joining appropriate text runs; simply drawing whole Qt paragraphs would move
text away from terminal-owned positions.

## First grid-preserving shaping integration

The isolated Qt terminal host now caches shaped/rasterized rows. Source-range
queries collect glyphs from full style runs; shared glyphs and Unicode grapheme
boundaries connect their original cells. Each group is translated back to its
source column while preserving internal shaping offsets. Wide spacers cannot
shift subsequent text. Glyphs are clipped at run/row bounds, not stretched.
Selection and background/style changes break runs; interaction and accessibility
continue to use Ghostty's original cells.

A local Qt 6.11.2 experiment exposed inconsistent `QGlyphRun::stringIndexes`
values across fallback segments. Those values remain diagnostic in the offline
report, but the integration uses `glyphRuns(from,length,...)` queries instead.
Qt warns that [glyph inspection is expensive](https://doc.qt.io/qt-6/qtextlayout.html):
it runs once per changed row/style/scale, not during paint.
[Raw fonts are thread-local](https://doc.qt.io/qt-6/qglyphrun.html), so glyphs are
rasterized on the GUI thread into immutable row images for Qt Quick painting.
Hidden/minimized windows skip this new work; reveal and device-pixel-ratio
changes refresh the raster cache. A bitmap per cached row adds CPU memory.

The same renderer produces `grid-*.png` alongside the preserved per-cell
`comparison-*.png` baseline. LTR fixtures require the full-text reference's
font/glyph multiset without dropping duplicate glyphs. Tests also protect wide
cell positions, joined emoji source spans, style boundaries, RTL policy, cache
retention and selection/scale invalidation. Native clipboard tests copy a shaped
operator back to its original `->` bytes; Wayland scale tests check the actual
row bitmap DPR along with window backing pixels, grid size and clipboard.

This is a bounded LTR integration. Runs containing RTL scalars (including
supplementary characters) or bidi controls keep the previous per-cell drawing
policy. Natural Qt paragraph reordering is not applied to the terminal. RTL
joining/order, fixed-grid Indic spacing, emoji width policy, tall fallback
clipping and box-drawing seams remain open. Native Mac integration acceptance
and one/fifteen-pane CPU/GPU/memory measurements remain required. The main
Workbench frontend and runtime are unchanged.

## Mac grid-shaping comparison result

On 2026-10-16 the owner supplied the integrated `grid-*.png` sheets, baseline
comparisons and report from Cocoa/Qt 6.12.0. All eight PNG dimension checks and
the report validator passed. The 1x, 1.25x and 2x integrated sheets were inspected.
An independent comparison of the reported font/glyph multisets matched the
full-text reference for all seven LTR specimens, including repeated combining
glyphs. The Arabic specimen reports unshaped per-cell placements, matching the
explicitly retained RTL policy.

Programming operator ligatures and joined emoji are visible in the integrated
Mac grid output. Indic glyph retention also passes, but its fixed-cell spacing
remains visibly uneven. The emoji source spans explain the large blank regions:
the scientist sequence occupies columns [0,6), and the family [7,15), despite
each now being drawn as a joined glyph. The integration preserves these source
positions rather than changing the runtime's width policy. This closes the
offline native Mac grid-renderer check, not spacing or language correctness.

Live Mac terminal selection/copy/resize, physical monitor sharpness and display
changes remain unconfirmed for the new row raster path. RTL shaping, Indic/emoji
width/spacing refinements, fallback clipping and box-drawing seams remain open.
The next toolkit gate is one/fifteen-pane CPU/GPU/memory measurement, including
retained/hidden behavior, before choosing this renderer for production.

## CPU row-raster/cache performance baseline

`just toolkit-perf` uses the actual shaped-row renderer and the same surface
cache/visibility early exit as the live Qt host. It builds a separate Release
target, runs 1/15 panes at fixed 80×24 geometry per pane, at 1x/2x raster scales,
and tests ASCII and mixed-Unicode Ghostty-derived grids. Each case has three
fresh processes, warmed samples, operation-count assertions and raw timings.
The method and limits are in the [benchmark procedure](../spikes/qt-input/README.md#cpu-rastercache-performance-mac-and-linux).

On 2026-10-16, Linux x86_64/build-machine (Qt 6.11.2, GCC 16.2.1, four reported
logical CPUs), the median CPU-path wall times per synthetic frame were:

| Profile / scale | Scenario | 1 pane (ms) | 15 panes (ms) | Ratio |
| --- | --- | ---: | ---: | ---: |
| ASCII / 1x | all rows changing | 5.685 | 88.338 | 15.54 |
| ASCII / 2x | all rows changing | 7.905 | 118.861 | 15.04 |
| Unicode / 1x | all rows changing | 3.994 | 62.093 | 15.55 |
| Unicode / 2x | all rows changing | 6.184 | 90.432 | 14.62 |
| ASCII / 2x | one visible, all sources changing | 7.983 | 7.879 | 0.99 |
| Unicode / 2x | one visible, all sources changing | 6.124 | 5.969 | 0.97 |
| ASCII / 2x | retained rows, forced compositing | 1.154 | 20.727 | 17.96 |
| Unicode / 2x | retained rows, forced compositing | 1.094 | 19.502 | 17.82 |

Retained and fully hidden sources performed zero new rasterizations. One-row
updates and full updates produced exactly their expected row counts; reveal
refreshed all pending rows. The one-visible case stayed close to the single-pane
cost, so hidden sources did not multiply rasterization. The remaining timing
includes synthetic source mutations and cache comparisons.

Fifteen panes retained 30.32 MiB of row images at 1x and 121.29 MiB at 2x. The
benchmark additionally allocated equal-sized CPU composite targets. The highest
observed child peak RSS was 288.37 MiB (15 Unicode panes, 2x); the ASCII case
peaked at 280.84 MiB. Peak RSS includes fonts, Qt and allocator overhead and is
not GPU/VRAM usage. Resources come from the exact child using Python's
[`wait4`](https://docs.python.org/3/library/os.html#os.wait4); Darwin's
[resource accounting](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/kern_resource.c)
uses resident-size bytes while Linux reports KiB.

The CPU image path is expensive under fully changing output; retained forced
compositing is also costly at 15 full-size panes/2x. These measurements justify
reviewing rasterization and copy/memory costs before frontend adoption. They
do not determine real application FPS: fifteen full-size surfaces multiply
the pixel area, and the benchmark excludes Qt Quick/GPU uploads, actual paint
scheduling, live backgrounds/cursor/chrome, PTYs, VT parsing and sockets. An idle
real terminal can skip painting entirely. Unicode/ASCII density differs, so
these profiles do not compare language-rendering speed.

This establishes the CPU/cache baseline. Native Mac measurements and live
Qt Quick/GPU/VRAM 1/15-pane validation remain open, along with external-screen
sharpness and the existing typography/RTL/spacing gates.

## First CPU shaping optimization

The Qt probe now batches consecutive equal-font glyphs into a single draw call
and uses a guarded bulk mapping for printable ASCII. Bulk mapping requires one
narrow scalar per cell, one primary-font run, a matching full source string and
valid monotone source indexes starting at zero. Non-ASCII, fallback and any
rejected mapping retain the existing source-range query path. Source positions,
style boundaries and glyph order remain authoritative.

An internal reference mode preserves the previous source-range mapper and its
per-glyph drawing. The text proof checks exact optimized/reference image equality
for all specimens at 1x/1.25x/1.5x/2x. Additional ASCII cases check source spans,
glyph IDs, font families and positions with normal/bold/italic and mixed
selection/color/underline styles. Reports expose whether the ASCII guard was
accepted; these flags do not add Workbench UI or persisted settings.

The previous CPU baseline is source `7d5543f`; its raw local artifacts were
retained under `.local/artifacts/qt-performance-7d5543f/` before repeating the
same Release 1/15-pane, 1x/2x, ASCII/Unicode workloads. Final comparisons use the
isolated run after regression testing, with three fresh-process trials per case.

On Linux/Qt 6.11.2/GCC 16.2.1, the final isolated run on 2026-10-16 gave these
median wall times for fully changing output (same fixed 80×24 geometry per pane):

| Profile / scale | 1 pane before → after (ms) | 15 panes before → after (ms) | 15-pane reduction |
| --- | ---: | ---: | ---: |
| ASCII / 1x | 5.685 → 2.624 | 88.338 → 42.309 | 52.1% |
| ASCII / 2x | 7.905 → 4.911 | 118.861 → 74.129 | 37.6% |
| Unicode / 1x | 3.994 → 3.775 | 62.093 → 58.797 | 5.3% |
| Unicode / 2x | 6.184 → 5.883 | 90.432 → 86.816 | 4.0% |

The plain ASCII specimen reports bulk mapping accepted. The Unicode gains are
modest; retained forced compositing stays around its previous baseline (about
20 ms for 15 panes/2x). Hidden/retained raster counts remain zero and all dirty
row/reveal assertions pass. Row-image storage is unchanged at 121.29 MiB for
15 panes/2x; highest child peak RSS was 294.40 MiB in this run versus 288.37 MiB
in the earlier baseline, so this is not a memory reduction.

The optimized/reference pixel, glyph and source-span checks passed, including
normal/bold/italic and selection/color/underline boundaries. Native Linux terminal,
Wayland scale/clipboard, real IBus/Anthy, AT-SPI, standard-control and full
`just check` regressions passed. Native Mac equivalence/timings remain unverified
for this optimization. These CPU gains do not establish live Qt Quick/GPU FPS;
Unicode query overhead, raster/copy memory, the existing typography gaps and
physical-display acceptance remain open.


## Mac CPU benchmark evidence

The Mac output supplied on 2026-10-16 at 22:19 completes all 24 fresh-process
trials and the deterministic dirty/hidden/reveal checks. On AppleClang 21.0.0,
median fifteen-pane fully changing times were 42.45/59.95 ms for ASCII at
1x/2x and 44.87/65.04 ms for Unicode. Forced retained compositing was
2.48/9.74 ms and 2.47/11.08 ms respectively. One visible source amongst fifteen
stayed close to a single source. Highest reported child peak RSS was about 449.8 MiB;
individual trials varied substantially. The log does not record a source SHA,
so it proves a native benchmark run, not a before/after gain for `23373a5`.
Future benchmark logs record the runner checkout and tracked-change status.
This does not establish native pixel equivalence for the latest optimization,
Qt Quick/GPU costs, physical sharpness or monitor-unplug behavior.

## Retained row extent optimization

The Qt probe now allocates each foreground row image only through its last
written cell or wide-cell spacer. It preserves written spaces (including
underlines), full source-grid coordinates, the original image origin, style
clipping and raster DPR. A blank row retains a one-pixel-wide transparent image
so scale reporting remains valid. Background/selection fills and cursor drawing
still use the complete source grid. Full rows retain the original allocation.
The bounds check happens only for invalidated rows, before allocation; painting
does not scan alpha pixels or crop/copy images.

The full-width source-range renderer remains the independent mapping/drawing
oracle. Tests composite cropped images onto full transparent canvases and
require exact pixel equality at 1x/1.25x/1.5x/2x for all existing specimens.
Additional cases exercise growing/shrinking/empty/full rows, wide tails, written
spaces with underlines, bold/italic and selection boundaries. Comparison sheets
now paint the same cropped images used by the live host.

Performance measurements compare the same Release matrix against the saved
`23373a5` baseline in `.local/artifacts/qt-performance-23373a5/`. Row-image
allocation for fifteen panes at 2x falls from 121.29 MiB to 112.19 MiB for the
nearly full ASCII rows and 65.19 MiB for the shorter Unicode fixture. This is
content-dependent foreground storage; CPU composite targets, Qt Quick textures,
VT state and fonts are not reduced by this change.

The final isolated Linux/Qt 6.11.2 run after regression checks used three fresh
processes per case. Median wall times (ms, same fixed 80×24 area per pane):

| Profile / scale | Retained 15 panes before → after | Fully changing 1 pane before → after | Fully changing 15 panes before → after |
| --- | ---: | ---: | ---: |
| ascii / 1x | 5.070 → 4.869 | 2.624 → 2.529 | 42.309 → 41.380 |
| ascii / 2x | 20.473 → 19.625 | 4.911 → 4.756 | 74.129 → 72.202 |
| unicode / 1x | 5.140 → 4.318 | 3.775 → 3.574 | 58.797 → 55.136 |
| unicode / 2x | 19.611 → 16.407 | 5.883 → 5.234 | 86.816 → 77.312 |

Forced retained compositing improves about 4% for the ASCII fixture and 16% for
Unicode; this remains a modest copy-path improvement, not a solution to the
full Qt Quick performance gate. Highest observed child peak RSS was 271.86 MiB
versus 294.40 MiB at the saved baseline. RSS is an observation, unlike the exact
row-image byte counts; allocator/font/platform overhead can vary.

Exact pixel equivalence, native terminal input/copy/paste/ownership, Wayland
fractional scales, real IBus/Anthy on xcb and Wayland, AT-SPI and full `just check`
passed. Dirty/reveal row counts remain exact and hidden/retained raster counts
stay zero. Native Mac cropped-cache equivalence/timings remain unverified;
Qt Quick textures/GPU/VRAM, typography gaps and physical-display acceptance are
still open. The next performance gate is the live Qt Quick image/upload path
under one and fifteen panes, rather than extrapolating these CPU timings to FPS.


## Live Qt Quick painted-item measurement

`just toolkit-scene-perf` adds a native Release scene workload for Mac/Linux,
with three fresh processes per 1/15-pane ASCII/Unicode case. Synthetic grids
come from the same pinned Ghostty fixture as the CPU benchmark; grid painting
is shared with the live terminal host, including full-width backgrounds. Pixel
checks protect unwritten-cell backgrounds, source offsets and fractional scales.
The regular Workbench frontend is unchanged.

The 1120×680 viewport contains full-resolution 80×24 painted-item surfaces:
one item at full scale or fifteen items displayed as thumbnails. Fifteen items
therefore retain fifteen times the source pixel area, despite the fixed window.
The workloads distinguish cached scene redraws, unchanged item repaints,
one-row/full updates and one visible item amongst fifteen changing sources.
Three warmup frames precede each timed sample. Exact row/paint/reveal counts
must pass; one generation remains in flight and stale frame callbacks are ignored.

The report separates GUI cache/update time, summed item-paint time, process CPU,
request-to-swap wall time, row cache bytes and exact-child peak RSS. Native scale,
graphics API, source checkout, environment, raw logs and rendered captures are
included. The estimated RGBA surface bytes are an allocation model, not VRAM.
There are no PTYs, sockets, input events, product chrome or arbitrary producer
backlog in this workload, and no GPU timestamps or allocator-level GPU readings.

Qt's [painted-item contract](https://doc.qt.io/qt-6/qquickpainteditem.html#paint)
keeps GUI state blocked while rendering reads the grid/images. Render callbacks
use direct connections and pass generation tokens back to the GUI thread.
The [frameSwapped signal](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped)
indicates queued presentation, so request-to-swap includes event-loop/vsync/
compositor pacing and must not be treated as GPU completion or input latency.
Backend startup logs use the documented
[scene-graph logging categories](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html#logging-support).

Native testing requires keeping windows visible, unobscured, on one display
and at a stable size. The run fails on premature close, missing frames, resize
or DPR change rather than reporting incomplete samples. Compare displays by
rerunning after the previous run finishes. Native Mac/backend and physical
external-display results remain open.

The final isolated Linux/Qt 6.11.2/GCC 16.2.1 test completed all 24 processes:
three trials per pane-count/profile/scale case. The API reports OpenGL with
`LIBGL_ALWAYS_SOFTWARE=1` on private Xvfb; no hardware GPU is exposed in this
environment. Row/paint/reveal counts pass in every case, and all captures have
the expected DPR-scaled window dimensions. The fifteen-pane capture was also
inspected: all fifteen sources are visible in the fixed viewport.

Median measured times (ms; summed paint work includes grid backgrounds):

| Profile / DPR | Cached scene request→swap, 1 / 15 | Item repaint request→swap, 1 / 15 | Full update request→swap, 1 / 15 | Full update paint, 1 / 15 |
| --- | ---: | ---: | ---: | ---: |
| ascii / 1x | 10.91 / 10.65 | 15.10 / 27.36 | 20.13 / 58.72 | 2.12 / 14.26 |
| ascii / 2x | 16.03 / 15.86 | 23.59 / 86.42 | 28.87 / 133.04 | 5.00 / 54.97 |
| unicode / 1x | 10.86 / 10.33 | 14.54 / 23.87 | 18.78 / 70.14 | 1.24 / 10.87 |
| unicode / 2x | 16.87 / 16.19 | 23.33 / 72.88 | 28.96 / 129.73 | 3.99 / 41.17 |

Cached scene samples perform zero item paints and zero row rasterizations;
repaint samples perform zero row rasterizations. One-row samples rerasterize
one row per visible source but still repaint the complete item, explaining why
their paint cost stays near full item repaint. Hiding fourteen changing sources
removes their row/paint work; revealing them updates exactly 336 hidden rows
and paints all fifteen items. A single-source reveal adds no dirty rows.

Highest process peak RSS was 549.54 MiB. That includes Qt/software-OpenGL
resources, fonts, warmup, reveal and screenshot readback/encoding as well as the
scene. It is not a steady-state allocation total or GPU memory measurement.
The software device and presentation pacing prevent extrapolating these timings
to native Metal/hardware-GL performance or a product frame-rate target.

`just check`, exact text/background pixel checks, native Qt terminal input/
copy/paste/ownership, Wayland fractional-scale checks and the existing CPU
benchmark matrix pass after sharing the fixture/paint function. Native Mac
`just toolkit-scene-perf`, Linux hardware runs, physical-display typography,
GPU timestamps/VRAM and full live-runtime load remain separate acceptance gates.


## Opaque background span optimization

The scene benchmark exposed substantial CPU work in full item repaints, even
when cached glyph rows did not change. The shared grid painter now groups
adjacent opaque cells with equal backgrounds into horizontal row spans.
A uniform 80×24 source grid drops from 1,920 grid-background fills to 24.
Background color changes still split spans; the backdrop, glyph images, cursor
and IME layer retain their original order and coordinates. This changes only
the Qt probe's presentation code, not runtime/session state or the main viewer.

The optimization applies only to non-antialiased opaque fills at full painter
opacity, ordinary source-over composition and axis-aligned transforms.
Other cases retain per-cell fills. Exact reference-image comparisons cover
four DPRs, two cell widths, fractional origins, color boundaries, transparency,
painter opacity, antialiasing, composition, rotation/shear and clipping. The
original scalar background compositor remains in the offline test.

The saved scene baseline is `.local/artifacts/qt-scene-f65cc77/`, captured before
this change. The repeated matrix uses the same Release 1/15-pane, ASCII/Unicode,
1x/2x, three-trial software-OpenGL workload and fixed geometry. These timings
remain CPU/scene-path evidence with presentation pacing, not native GPU FPS.

The isolated repeated run on Linux/Qt 6.11.2 completed all 24 software-OpenGL
trials before regression checks. Median unchanged-item repaint results (ms):

| Profile / DPR | Paint 1 pane before → after | Paint 15 panes before → after | Paint reduction, 15 | Request→swap 15 before → after |
| --- | ---: | ---: | ---: | ---: |
| ascii / 1x | 2.573 → 1.818 | 14.213 → 11.222 | 21.0% | 27.360 → 24.521 |
| unicode / 1x | 1.993 → 1.106 | 10.756 → 7.967 | 25.9% | 23.867 → 21.488 |
| ascii / 2x | 4.843 → 4.382 | 54.094 → 51.298 | 5.2% | 86.415 → 83.644 |
| unicode / 2x | 3.826 → 3.115 | 40.864 → 37.989 | 7.0% | 72.882 → 70.209 |

The fifteen-pane repaint paint cost falls 21–26% at 1x and 5–7% at 2x for these
uniform backgrounds. More pixel copying remains at 2x; this does not identify
GPU upload time. Total request-to-swap gains are smaller and not uniform across
every fully changing/single-pane sample. Native display pacing and software
OpenGL overhead are still included. This is a measured paint-path improvement,
not a frame-rate guarantee or a glyph/typography change.

Cached-scene frames still perform zero item paints and zero row rasterizations;
all dirty/hidden/reveal row and paint counts remain exact. Row-image byte counts
match the saved baseline for every case. No memory reduction is claimed.
Exact text/background pixel checks, native terminal input/copy/paste/ownership,
Wayland scale/clipboard checks and full `just check` pass. Native Mac pixel
and scene results for span painting remain unverified, alongside the existing
hardware GPU, external-screen and full live-runtime acceptance gates.


## Native Mac scene benchmark evidence

The scene artifacts supplied in `debug/qt` on 2026-10-16 identify clean source
`f65cc77`, Cocoa/Qt 6.12.0, AppleClang 21.0.0, and a native 2x display. All twelve
trials (three per ASCII/Unicode and 1/15-pane case) pass raw/aggregate equality,
exact row/paint/reveal counts and 2240×1360 capture dimensions. Startup logs
identify the threaded render loop and Metal on **Apple M2 Pro**. The fifteen-pane
Unicode capture was inspected; all fifteen sources render and reveal.

Median request-to-swap and summed item-paint times, in ms:

| Profile | Cached scene swap, 1 / 15 | Repaint swap, 1 / 15 | Repaint paint, 1 / 15 | Full update swap, 1 / 15 |
| --- | ---: | ---: | ---: | ---: |
| ascii | 8.28 / 8.92 | 8.31 / 71.41 | 3.90 / 52.08 | 16.68 / 120.41 |
| unicode | 8.67 / 8.81 | 8.34 / 52.57 | 3.20 / 34.39 | 12.75 / 102.57 |

Cached scene redraws perform zero paints. One-row changes still repaint full
items (about 34–52 ms of paint work across fifteen panes), while one visible
source amongst fifteen uses exactly one item's paint and 24 rows per frame.
The GUI cache-update and item-paint measurements account for substantial work
before presentation; this supports reducing CPU paint and invalidated area,
without claiming that Metal upload or GPU execution have been isolated.

Highest child peak RSS was 839 MiB, including warmup/reveal/capture phases and
Qt/Metal/font resources. This is not steady-state memory or measured VRAM.
Memory acceptance remains open. The run exercises current shaping/cropped caches
in the scene path but does not execute the exact text pixel oracle, physical
external-display sharpness checks, full live PTY load or background-span changes.

The logs include a non-fatal malformed logging-rule warning: the runner joined
`QT_LOGGING_RULES` with newlines. It now uses semicolons, preserving inherited
rules. The benchmark still completed and the API/device metadata are valid.
A Linux software-OpenGL diagnostic run verifies the corrected logging syntax.


## Changed-row repaint integration

The Qt probe now feeds changed-row damage from the retained foreground cache to
`QQuickPaintedItem::update(rect)`. Initialization, geometry/font/DPR changes,
global background changes and active preedit retain full repaint requests.
Cursor-only refreshes damage the old/new cursor bounds; completely unchanged
refreshes request no paint. Explicit full rectangles preserve full damage when
later row updates are coalesced in the same event-loop turn. Resize and DPR
events also request full bounds immediately, before asynchronous PTY/cache work.
These are presentation facts; runtime/session state and the main viewer are
unchanged.

Row bands cover the full width plus vertical edge padding, preserving erased
suffixes, underlines and fractional raster coverage. The pure grid painter draws
only rows intersecting Qt's clip. The benchmark's one-row case therefore draws
three row bands including neighbor edges, rather than all 24. New exact
`painted_grid_rows` checks complement existing raster/paint/reveal counts.
Ordinary repaint and fully changing scenarios still repaint the full item.

The [documented rectangular update API](https://doc.qt.io/qt-6/qquickpainteditem.html#update)
schedules visible item redraws. Review of the
[Qt 6.11 painter node](https://raw.githubusercontent.com/qt/qtdeclarative/6.11/src/quick/scenegraph/util/qsgdefaultpainternode.cpp)
confirms CPU clipping, but does not establish a corresponding reduction in GPU
texture upload. No private Qt APIs or upstream code are copied into the probe.
Backing image/texture allocations are unchanged; memory/VRAM acceptance remains
open.

The offline oracle retains a canvas through content changes, line erases,
selection colors, cursor move/hide/show and multiple updates before presentation,
requiring exact equivalence to fresh full paints at all four scales. It also
checks full invalidation on font/resize/hidden-DPR changes. The scene benchmark
captures the actual one-row result, forces a full repaint of identical state,
and requires exact capture equality. Readbacks and the oracle frame run outside
timing. Native Mac `toolkit-text-test` and `toolkit-scene-perf` remain required
for this integration.

The isolated Linux/Qt 6.11.2 matrix completed 24 fresh-process software-OpenGL
trials, three per 1/15-pane, ASCII/Unicode and 1x/2x case. Every trial requires
`partial_matches_full: true` and exact row/paint/reveal counts. A one-row frame
rerasterizes one row and issues grid draw commands for three bands per visible
pane, including the clipped neighboring edges; full paints draw all 24.
The saved pre-change matrix is `.local/artifacts/qt-scene-1021b29/`.

Median one-row update results (ms):

| Profile / DPR | Paint 1 pane before → after | Paint 15 panes before → after | Request→swap 15 before → after |
| --- | ---: | ---: | ---: |
| ascii / 1x | 1.923 → 0.075 | 11.046 → 0.548 | 24.867 → 12.365 |
| unicode / 1x | 1.291 → 0.088 | 7.788 → 0.421 | 21.911 → 12.734 |
| ascii / 2x | 4.620 → 0.221 | 51.299 → 1.863 | 83.845 → 24.780 |
| unicode / 2x | 3.114 → 0.144 | 38.399 → 1.226 | 72.217 → 24.488 |

Fifteen-pane paint time for this small-update workload falls about 95–97%.
Total request-to-swap time includes pacing and upload work and falls less.
This is not a claim about fully changing terminals. The initial Unicode/1x full
case was slower than its saved baseline, so it was investigated using an isolated
Release rebuild of `1021b29` and the current candidate, alternating AB/BA/AB with
800 ms samples on the same private display. Full-output median GUI/paint/swap
were 50.72/7.69/66.23 ms before
and 51.74/7.73/67.16 ms after;
the frame ranges overlap and the large slowdown did not reproduce. Raw reports
are in `.local/artifacts/qt-scene-full-ab/`. No full-output speedup is claimed.

Row-image bytes are exactly unchanged across the matrix. The extra pixel-oracle
readbacks affect process peak-memory accounting, so a lower RSS is not claimed.
Full-size item images/textures, GPU upload costs and VRAM remain separate issues.

Exact text/background/retained-image checks, real terminal input/copy/paste/
ownership, Wayland fractional scales, real IBus/Anthy on xcb and Wayland, AT-SPI
and `just check` pass. Native terminal/Wayland/IME checks were rerun after making
resize/DPR/composition full invalidation use explicit bounds. At that stage, native Mac pixel and scene evidence, physical-display typography
and full live-runtime acceptance remained open; the subsequent upload is recorded
below.


### Mac changed-row evidence and word navigation

The clean `d1a195e` Mac upload (Qt 6.12, Cocoa, Metal on M2 Pro, 2x)
passes all twelve partial/full scene capture comparisons. The refreshed text
artifacts pass validation and include the ASCII fast-path diagnostics.
Fifteen-pane one-row paint medians are 2.303 ms for ASCII and 1.478 ms for
Unicode; request-to-swap medians are 17.314 and 16.927 ms. Fully changing output,
physical-display typography and live-runtime memory/VRAM acceptance remain open.

The terminal probe maps Option–Left/Right on Mac and Alt–Left/Right on Linux
to the same shell word-motion sequences Escape-b/f. Additional modifiers bypass
this binding and retain terminal encoding. `just toolkit-terminal-key-test` sends Qt key events
through the actual host and a disposable raw PTY, checking plain/control arrows,
shared word motion, repeat and release suppression. It writes
`.local/artifacts/qt-keys/`. Native Mac verification remains required.

For native Mac and Linux acceptance, run `just toolkit-terminal-key-test`, then
`just toolkit-terminal`. At a shell prompt type a few words and use Option–Left
and Option–Right (Alt on Linux), including holding either arrow: the cursor should move by
words without inserting letters. The automated test saves actual/expected byte
hex and `key_bytes_match` in `.local/artifacts/qt-keys/report.json`, including
byte mismatches, so native failures can be inspected without retyping input.
