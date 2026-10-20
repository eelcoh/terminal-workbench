# Production toolkit readiness

Status: 2026-10-18. Keep the approved minimal chrome and Overview interaction.
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


### Editable-terminal IME transfers

`just toolkit-terminal-ime-transfer-test` adds a second independently owned
runtime and raw PTY reader to the private IBus/Anthy test. Each Qt window is
editable, with its own input context and byte capture. The original
`toolkit-terminal-ime-test` retains the refused-owner activation case.

The new proof passes with IBus 1.5.34/Anthy on both xcb and native Wayland in
private Niri (with DISPLAY unset for Qt). Two round trips switch real window
focus while composition is pending in each terminal. Preedit cancels without
PTY input. The peer then converts and commits 日本 and にほん exactly once,
and receives Return/Ctrl-C independently of the first terminal. After the peer
window detaches, the first terminal completes its original conversion, commit
and key checks. PID and raw-byte checks require both reader processes to stay
alive and unchanged and exclude cross-terminal input throughout.

Artifacts are `.local/artifacts/qt-ime-transfer/{xcb,wayland}/`: result JSON,
per-window reports, raw bytes for each PTY and backend logs. This covers separate
editable windows on Linux, not two items sharing one window's input context.
Native Mac editable-terminal transfers, same-window transfers and candidate
panel placement remain separate acceptance work.

The Linux cancellation result is backend-specific evidence. Do not require
identical cancellation on Mac before deciding whether native commit-on-blur is
acceptable; the focus policy remains open. See the native observations below.

The owner also confirmed Alt–Arrow word navigation in the Linux Qt terminal
probe after the shared Option/Alt binding change. Native Mac word navigation
still needs direct confirmation.


### Same-window terminal composition and toolbar routing

`just toolkit-terminal-pair` opens two disposable runtime-owned shells side by
side in one Qt window on Mac/Linux. Clicking a pane changes the input target;
Copy/Paste preserve the selected pane when the toolbar itself takes focus.
It is an input fixture, not a replacement for Workbench's production chrome.
Closing the interactive fixture stops both of its disposable shells.

`just toolkit-terminal-ime-pane-test` extends the private Linux IBus/Anthy
harness to this shared window/input context. Both xcb and native Wayland pass
two round trips with pending composition in A and B. Preedit cancels without
PTY bytes; conversion, repeated commits and ordinary keys reach only the
focused terminal. Toolbar Paste after B focus appends a distinct clipboard
marker only to B. Both raw readers retain their PID and stay alive while A
completes its own input checks. No new composition-reset policy was needed.

Reports are `.local/artifacts/qt-ime-pane/{xcb,wayland}/`: `terminal.json`
contains A plus the nested `peer` report for B. Each reader's raw bytes, native
backend logs and window captures are retained. The original refused-owner and
separate-window tests remain available and pass after the report refactor.
Single-terminal report fields keep their existing meaning.

The owner confirmed same-window copy/paste on Mac. Japanese–Romaji can compose
にほん and convert it to 日本 with Space. Pending text appeared more widely
spaced after changing panes; exact commit/cancel ownership was not captured.
This is an observation, not proof of a Mac cancellation defect. Native focus
policy and candidate placement remain open, without another manual Japanese
acceptance loop. Japanese exercises composition mechanics; it does not establish
Hindi, Telugu, other input engines or general complex-script rendering support.
Keep the automated Linux isolation coverage and plan broader language coverage
separately from production UI work.

### Live-runtime performance

`just toolkit-live-perf` measures CPU and RSS with one and fifteen real runtime
sessions, raw PTY producers and the Qt terminal client. The private software-GL
matrix passes byte/marker agreement, idle/hidden presentation isolation and exact
reveal catch-up at 1x/2x in three fresh trials each. See
[live workload and measured limits](toolkit-live-performance.md).
The owner supplied a passing clean d4b8ce4 Mac/Metal run: all six live trials
pass the same data-path checks. Fifteen-pane full-output viewer RSS is about
769 MiB and median lifetime peak 877 MiB; memory remains an optimization concern.
Physical external-display typography, remaining input/accessibility acceptance,
packaging and review of the production design slice still gate adoption.
The main viewer is unchanged.

### First Qt UI design slice

`just toolkit-ui` opens the approved minimal chrome direction around six
real disposable shells, with a workspace drawer, pane indicators, scrolling
strip, per-pane width presets and a spatial all-workspace Overview. It uses
transforms of the same live terminal items rather than resize/reconnect during
Overview entry. Offscreen/background terminals retain parsing but skip row
rasterization and painting; revealing them catches up from current state.

Native Linux xcb and Wayland tests pass keyboard routing, per-pane width and
full restore, remembered workspace focus, live hidden output, Overview click/
Escape, empty-workspace navigation and unchanged terminal grids/PIDs during
Overview. This is the first interaction slice, not complete frontend adoption.
Mac UI review, physical trackpad input and remaining product flows stay open.
See [UI study scope and review](toolkit-ui-study.md).


### Qt study creation through the existing service

The owner confirmed the initial study on both Mac and Linux. The creation slice
uses the established service methods and a private catalog, with asynchronous
requests, native editable name/folder fields, inline validation, busy/capacity
controls and creation/navigation shortcuts. New identities append live items;
existing terminals are not reconnected or recreated. Overview wraps additional
terminals without resizing their PTYs. Native xcb/Wayland tests exercise creation,
invalid folders, cancellation, cwd inheritance, original-reader isolation and
normal/failed-launcher cleanup. Native Mac creation and later theme/drag flows
remain acceptance work. See [the study](toolkit-ui-study.md).


### Workspace Appearance in the Qt study

The owner confirmed the creation flow on both Mac and Linux. The next slice
reuses the existing Ghostty theme parser/catalog for workspace preview and
Apply/Cancel, matching chrome and new-terminal inheritance. Picker colors stay
steady; appearance remains client state without catalog mutation, reconnect or
PTY geometry changes. Native xcb/Wayland checks inspect actual indexed/RGB/OSC
cell colors and exercise workspace/chrome isolation, cancellation and Apply.
Mac Appearance acceptance, saved settings and imports remain open. See
[the study flow](toolkit-ui-study.md#workspace-appearance).


### Cursor focus in the Qt host

The owner confirmed Appearance and reported identical hollow cursors in active
and inactive panes. The host now consumes Ghostty's visual cursor style,
distinguishes terminal/window focus, and keeps text readable under a filled
block through a small retained glyph mask. Native xcb/Wayland pixel checks pass
filled/outline focus, glyph preservation, application bar/underline and hidden
cursor behavior, with existing readers/grids preserved. Mac review of this
polish remains open; no blink timer is added.


## Qt study: Overview drag and reorder

The owner confirmed cursor styling works. Plain clicks in the terminal now clear
selection without selecting the clicked cell; deliberate text drags retain
selection/copy behavior. Overview previews can be dragged to reorder or move
between workspaces, including empty ones. A destination outline/insertion marker,
Escape cancellation and edge scrolling guide the move. Full destinations reject
moves while allowing same-workspace reorder.

The UI submits the existing `terminal.move` service command, then projects the
confirmed catalog onto stable terminal items. Width preferences, live shells,
cwd and attachments survive; themes follow the destination workspace. Keyboard
navigation and focus memory follow the confirmed order. Native xcb/Wayland checks
cover drag, cancel, empty destinations, edge scrolling, theme inheritance,
reordered navigation and unchanged grids, raw-reader PIDs and input isolation.
Native Mac click/drag review and keyboard-only move commands remain open.
This is still the disposable Qt study, not a production frontend switch.
See [moving terminals](toolkit-ui-study.md#moving-terminals-in-overview).


## Qt study: workspace name and folder editing

The working-view application menu now exposes Rename workspace and Workspace
folder as separate Save/Cancel forms, reusing the creation fields. Existing
values retain their text with the caret at the end for in-place edits. Empty
values disable Save; invalid folder errors stay inline. The UI submits existing
`workspace.rename`/`workspace.directory` methods against the private study
service. Each form saves one setting without a combined partial-success path.

Confirmed catalogs update the workspace labels and the folder used by new
terminals. Existing terminal items, themes, width preferences, grids, input
ownership and shell cwd/PIDs are retained. Empty workspaces can be edited without
spawning shells. Native xcb/Wayland checks exercise edits, cancellation, invalid
folders, empty workspaces and actual new/existing-shell cwd behavior alongside
the existing drag, input, cursor and appearance coverage.

Native Mac workspace-editing review remains open. Native folder browsing, agents,
keyboard-only moves, saved configuration and terminal lifecycle/ownership UI
remain later slices; this does not switch the production frontend.
See [editing a workspace](toolkit-ui-study.md#editing-a-workspace).


## Qt study: folder browsing

Workspace creation and folder editing now expose Browse beside the path field.
Qt Quick Dialogs supplies the platform picker, with its standard fallback when
native dialogs are unavailable. A missing typed folder starts browsing from its
nearest existing parent. Choosing fills the form only; Create/Save performs the
existing service request. Picker cancellation retains typed text and the parent
form, while cancelling the form discards an accepted choice.

Local file URL conversion preserves spaces, Unicode and URL-special characters.
Native xcb/Wayland input checks drive Qt's standard fallback independently of
host portals, exercise keyboard entry, cancellation, selection and no implicit
writes, then verify a real new shell starts in the selected Unicode folder.
The owner confirmed the native Mac picker works. Physical Linux picker review
remains open; no saved production
catalog or renderer adoption is introduced. Qt Quick Dialogs is an existing Qt
module, linked explicitly by the UI study target.
See [folder browsing](toolkit-ui-study.md#browsing-for-a-folder).


## Qt study: keyboard pane moves

Add Shift to Cmd–Option on Mac or Super–Alt on Linux: Left/Right reorder the
selected terminal, Up/Down append it to the adjacent workspace and follow focus.
Boundaries do not wrap, full destinations show a brief notice, and pending move
requests consume repeat shortcuts without sending them to a shell. Pane menus
and the scrollable shortcut overlay expose the same actions. Overview follows
keyboard moves without changing PTY geometry.

Native xcb/Wayland checks pass reordered navigation, focus following, workspace
appearance, empty-source memory, full/boundary no-ops, Overview moves, menu access,
stable grids and original-reader PID/cwd/input isolation. Native Mac keyboard
move review remains open. Folder browsing is owner-confirmed on Mac.
See [keyboard review](toolkit-ui-study.md#keyboard-review).


## Qt study: TerminalColors and Ghostty imports

Appearance now imports a local Ghostty file or a chosen TerminalColors page/direct
download link, with optional name and attribution. Import validates and saves an
offline palette, then previews the current workspace through the existing
Apply/Cancel workflow. Source/raw text/license notes are retained in a separate
configuration theme collection; no downloaded external palettes are bundled.

The existing bounded color parser is reused. Non-color settings never execute.
Exact duplicates, corrupt state, concurrent-window merges, failed/redirected or
oversized downloads and cancellation are covered without changing running shells
or existing themes. Built-ins remain usable if saved imports are invalid.

Cross-platform offline model/store/network tests and native xcb/Wayland UI checks
pass. The real Nord Ghostty export and a live TerminalColors page link passed
Qt HTTPS + parsing + disposable caching on 2026-10-18. Native Mac import acceptance
remains open; this adds palette import, not a production frontend switch.
See [imports](toolkit-ui-study.md#importing-terminalcolors-and-ghostty-themes).


## Qt study: runtime captions and agent Overview

The Qt candidate now consumes the existing read-only context and agent lifecycle
subscriptions. Pane and native-size Overview captions show directory/Git/branch/
linked-worktree context, with remote/unavailable fallbacks. Agent labels expose
Working, Needs input, Ready and Status unknown without interpreting terminal text.

The rail has an Agents entry and Overview has explicit, visibly active identity
and attention filters plus empty-result messages. Filtered previews use a cached
projection of the real catalog; terminals keep stable items, readers and grid
sizes. Drag insertion maps back to real ordinals rather than reordering hidden
terminals accidentally.

The metadata adapter is bounded, validates user-owned control sockets, deduplicates
updates and isolates malformed/disconnected/timed-out feeds from terminal input.
It does not attach PTYs, start runtimes, poll or launch Git. Native Mac review and
production adoption remain open. See
[live context and agents](toolkit-ui-study.md#live-context-and-agents).

## Session service shutdown acknowledgement

The full check exposed an immediate shutdown/reopen race: a caller could receive
the shutdown reply and connect to the still-listening socket before its service
exited, leaving a request that would never be accepted. Shutdown now closes and
retires its own endpoint and releases service ownership before acknowledging.
The existing no-replay rule for mutations remains intact. Runtime shells survive
service-only shutdown. The shutdown/reopen, lifecycle, concurrency and soak checks
pass, including an assertion that the endpoint is retired before acknowledgement.


## Runtime hardening: short input bursts

Expanded native UI tests exposed a healthy terminal losing its connection during
a short typed command. The runtime's PTY writer queue counted messages: sixteen
single-key packets could exhaust it while the writer was briefly unscheduled,
even though only a few bytes were pending.

The writer queue now retains its 1 MiB byte budget, including a packet held by a
blocked writer, while allowing up to 4096 small packets. A separate message limit
keeps descriptor memory bounded. Failed sends, receiver shutdown and completed
writes release their reservations; input remains ordered, and genuine queue
exhaustion still isolates the attached sender. No protocol or saved state changes.
Transport/event queues retain their own existing bounds.

Deterministic tests hold the writer back while queuing 2048 single-byte packets,
exercise byte/message limits and confirm budget release. The real PTY regression
sends those 2048 packets followed by 120 KiB of paste data, verifies exact ordered
bytes, then checks the same reader/attachment accepts a resize. It runs as part
of `just check`. The capability fixture also waits for its one expected request,
removing an unrelated background-thread exception from otherwise passing checks.


## Qt chrome aligned with the working viewer

The Qt study now uses the spike's bundled Adwaita Sans for interface text,
with native toolkit text rendering and separate terminal typography. Original
geometric icons replace font-dependent symbols. The compact 40-pixel top bar
and 44-pixel rail restore the left menu placement, workspace label, centered
pane-position pill, focused/count text and adjacent New terminal action.

Chrome colors follow the same light/dark mixing rules as the working viewer,
including cursor-derived accents and semantic attention colors. Menus, section
separators, check marks, disabled actions, tooltips and workspace rows use shared
controls. Long names elide; eight indicators, count and actions fit the minimum
680-pixel window. Agent attention has a rail count and pane-position dot, with
status text retained separately in the pane header. Idle previews use an arrow;
the grab cursor appears during dragging only.

This ports the approved visual direction into `just toolkit-ui`; `just spike`
remains the daily-use frontend. Native Mac visual acceptance is still required.
The UI suite checks actual bundled font selection, minimum-width non-overlap,
long workspace names, attention transitions, existing keyboard/menu/focus flows,
and stable terminal identities/geometry through overlays. Review captures include
`chrome-menu.png`, `chrome-workspaces.png` and `chrome-narrow.png` under
`.local/artifacts/qt-ui/{xcb,wayland}/`.


## Qt terminal lifecycle and ownership

The Qt study now shows a compact status card when a terminal is disconnected.
Recorded exit codes/signals, explicit stops and missing-exit evidence come from
the existing session service. Unknown evidence stays unknown. Status checks are
bounded, coalesced per terminal and request driven; they never start a missing
service, replace a shell or attach a PTY owner. Check again refreshes those facts.

- **Reconnect** attempts a normal attachment to the running process. If another
  window owns it, that window keeps control.
- **Move here…** asks for confirmation before transferring the existing process.
  The old owner receives an explicit release state and can later reconnect.
- **Start new shell** is offered only for a stopped runtime. The existing service
  revalidates that no shell is live and starts in the workspace's configured
  folder. It preserves terminal identity, theme and width; it starts a new process.
- **Close terminal…**, in the pane menu, asks before stopping the process and
  removing its catalog entry. Cancel receives initial focus; Enter cannot silently
  confirm destruction. Other terminals retain their live items, PIDs and input.

Snapshot reconnect commits a newly decoded client replica and rebuilds its render
handles without taking terminal authority from the runtime. A failed snapshot
keeps the old display; failures stay local to that terminal. Removed panes retire
their context/agent/status observers. Selection, widths and remembered workspace
focus are reconciled by stable identity rather than retaining shifted indices.

This slice also fixes returning from Overview with an obsolete animated scroll
position. Reveal uses the intended scroll target and clamps to the current
workspace; a disconnected selected terminal's controls remain reachable.

`just toolkit-lifecycle-test` exercises lifecycle-only requests, fragmented,
malformed, oversized, closed and timed-out replies, missing-service non-start and
observer retirement without a display. The native UI suite uses real runtimes to
exercise exit 42, missing evidence, new-shell recovery, explicit ownership transfer,
non-stealing reconnect, service downtime and confirmed/cancelled close. It verifies
stable sibling UI instances and reader PIDs, then the normal narrow-window layout.

For manual Mac/Linux review, run `just toolkit-ui`, enter `exit 42`, inspect the
reason and choose Start new shell. Open that pane's overflow menu, choose Close
terminal…, cancel once, then confirm. The workspace should become empty when its
last terminal closes; its neighbours should keep running. Native Mac acceptance
of this slice remains pending. Paired-window saved-session integration remains a
later slice; the study launcher still owns only private disposable sessions.


## Qt command access

The candidate adds a themed native-control command palette, context reasons,
shortcut hints and a client-only action registry. Shared creation/layout dispatch
reuses existing handlers; close reuses the existing confirmation. Native X11 and
Wayland coverage exercises input isolation, unavailable/no-match choices, safe
dialog handoff, mouse/Tab navigation and stable terminal instances/grids. Native
Mac palette acceptance remains open. Font/configuration, saved sessions and
packaging remain before a frontend decision; `just spike` remains the daily viewer.


## Qt font and configuration slice

The candidate adds bundled JetBrains Mono/Fira Code, native logical-point
sizing, preview/apply/cancel/defaults and scoped size shortcuts. Startup font
defaults reuse the current managed shared/platform parser. Configuration stays
read-only; invalid input falls back with a dismissible notice. Window changes
remain temporary. Core contracts cover precedence, symlinks and transaction
boundaries. Native GUI coverage checks six-pane preview/cancel geometry,
stable processes/items/input, modal scope, new-pane inheritance and
minimum-width controls. The owner confirmed Mac font selection and sizing.
Font-only loading now skips unused external theme files while retaining
existing field validation; full theme validation remains in the working
viewer. The warning fix needs a Mac recheck, and mixed-display acceptance
remains open. Remaining managed settings, saved window preferences and
packaging still precede adoption.

## Managed Qt shortcut loading

Supported managed application chords now load with shared/platform precedence,
exact Command/Control mapping and dynamic hints/help. Invalid/conflicting settings
retain built-in bindings with a notice. Native clipboard is reserved; unsupported
actions are unintercepted. Qt-only move defaults yield to configured actions.
Core tests cover both platform mappings and failures; private X11/Wayland tests
exercise real remapped keys, old-chord passthrough and modal isolation. Native Mac
acceptance, the editor/recorder, UI override loading and forwarding remain open.

The shared C adapters' header helpers are now inline rather than unused static
functions. They compile with `-Wall -Wextra -Werror` in build-machine, addressing
the reported Mac unused-function warnings without disabling diagnostics. The
optional Vulkan discovery message is separate from those compiler warnings.


## Drop-in theme discovery

Appearance now scans the configuration `themes/` directory at startup/opening,
including TerminalColors `.default` downloads and extensionless Ghostty files.
An Open themes folder text link opens the native location, with an explanation
that files appear automatically when Appearance reopens. The import form is no
longer exposed; existing imported themes remain available. Source files stay read-only;
no import manifest or network request is needed. Invalid files are isolated,
unchanged scans reuse registry slots, and active colors survive edits/removal.
Core and native UI coverage exercise these boundaries. Mac folder/discovery
acceptance remains open; see [the directory workflow](theme-directory.md).

## Qt shortcut editor

The recorder/editor adds client-only sparse UI shortcut persistence using the
existing `workbench-shortcuts-v1` layer. It reuses the bounded parser/validator,
reserves native clipboard and shell controls, and keeps live bindings separate
from drafts. Apply uses a short lock and existing atomic writer, refuses stale
managed/UI bytes and refreshes registry hints. Model coverage includes both Mac
modifier meanings and native Linux checks drive recording/Cancel/Apply/defaults
without leaking keys or replacing terminal readers. Native Mac acceptance remains
pending. Forwarding, unsupported actions and saved-session state are separate.

## Qt one-shot key forwarding

Forwarding is client-only and scoped to an already connected focused terminal.
ShortcutOverride acceptance bypasses Qt application shortcuts for one chord;
press/repeat/release then use the normal mode-aware terminal encoder while native
clipboard and host word-motion shortcuts yield. Pending requests retire on a
ten-second timer, Escape, pointer/focus/visibility loss, composition or disconnect.
Native input coverage uses real PTY readers for legacy F10, native Copy bypass,
Kitty F10 press/release, menu focus handoff, cancellation and ordinary interception
restoration. Existing terminal-key checks remain required. Mac review is pending.

## Qt durable font defaults

Font Apply now commits sparse per-platform UI defaults separately from managed
configuration and session data. Bounded/versioned loading falls back cleanly on
invalid saved state and never overwrites that invalid file. QSaveFile and a short
lock protect commits; stale managed/UI bytes reject saves without closing the
dialog. Model tests cover both platform bases, managed symlinks, save/reopen,
sparse inheritance, temporary sizing, invalid/oversized state, write failures,
locks and stale updates. Native Linux UI checks verify durable bytes, cancellable
preview/reset, real PTY grids and unchanged terminal items/readers. Native Mac
save/relaunch review remains pending.

## Config-root parity and saved-session launch

Mac/Linux preference discovery now shares the XDG default under `~/.config`.
Mac migration tests cover missing-only copies, theme/import/font preservation,
symlinks, existing destination choices, explicit-root exclusion and once-only
behavior, without copying sessions. Saved Qt launch is an explicit alternative
to the disposable study. Real-runtime launcher tests retain catalogs, same shell
and service PIDs after normal/failed viewer exits, preserve stopped entries and
remove only temporary viewer config. Native UI regressions retain disposable
cleanup. Saved Qt view/theme state and automatic catalog updates are implemented. Broader
Mac acceptance remains pending; this is not production frontend adoption.

## Qt leased view restoration

The Qt launcher reuses the existing descriptor-held slot lease helper under its
own `qt/windows` namespace; it never reads/writes spike view codecs. The client
saves a bounded versioned JSON profile through atomic replacement. Typed model
coverage exercises identity maps, fractional layout/scroll, filter booleans,
invalid/future/oversized/stale preservation and local write failures. Theme
snapshots preserve all 256 palette entries plus defaults/selection/cursor; restored
colors remain available if the original theme file was removed. Saves use a
cached color snapshot and a 350 ms client-state debounce, without parse/render I/O.

Launcher checks prove independent concurrent slots and reuse with retained
shell/service PIDs. Native Linux UI checks restart the viewer after changing
width/full-width, focus, Overview/motion and a directory theme, then remove the
source and verify unchanged colors, input, geometry and reader PIDs. The remaining
existing UI/ownership/lifecycle flows still run. The owner confirmed appearance
retention with `f80f00b`; broader Mac physical display/view review remains pending.

## Qt catalog invalidation and read-only projection

A separate QtCore/Network feed watches the catalog file and its directory,
rearming after atomic replacement. Coalesced notifications request `catalog.read`
from the owned service socket. No PTY, process tree, startup/recovery or render-loop
work is introduced. Snapshot validation, size/deadline limits, duplicate suppression
and pause/resume around local mutations keep failed or stale reads local.

Model checks cover fragmented responses, replacement notifications, local mutation
races, unsupported services, invalid/oversized catalogs, timeout and absent service.
Service checks verify advertised read-only access with unchanged file bytes/mtime
and no startup on absence. Native X11/Wayland suites use two simultaneous Qt windows
for rename, folder, creation, move and deletion, including deletion during editing;
original readers, input and ownership survive. Runtime-facts regressions pass after
sharing the existing owned-socket check. Native Mac catalog-sync review is pending.

## Qt pinned navigation

One panel moves between the temporary drawer and a reserved column; pinning does
not introduce a second terminal model or runtime subscription. Its Workspaces and
Agents tabs share existing read-only metadata and lifecycle observers. Stable
agent membership avoids rebuilding rows on status-only changes, retaining keyboard
focus. Selecting an agent focuses its existing terminal across workspaces without
transferring ownership or changing Overview filters.

The optional typed `navigation_pinned` view field accepts existing profiles with
no pin preference. Model checks cover old profiles, roundtrip and invalid types.
Native Linux UI checks cover pin/unpin, workspace keyboard navigation, narrow
fallback, restored pin/layout/theme state and cross-workspace agent focus. Live
status-label checks use the controlled runtime process/hook fixture. Native Mac
input, layout and saved-pin review remain pending.

## Qt retained-runtime compatibility

Linux saved-session inspection found runtime sockets without a control endpoint,
plus earlier controls that do not advertise capabilities. The Qt host had required
snapshot attachment unconditionally. It now follows the working viewer’s bounded
legacy fallback only before any response: missing/refused control or clean EOF
on an unsupported request. Rejections and partial/malformed responses retain their
failure, never silently downgrading. Transfer fallback requests ordinary attachment
and cannot revoke an older owner. Replay input/query replies wait for Ready and
history is limited to 16 MiB. No runtime is restarted or replaced.

Disposable framed-server checks exercise missing/refused/unknown controls,
fragmentation, restored text, exact keyboard bytes, suppressed historical query
replies, owner rejection and partial-response isolation. The existing real-PTY key
suite and native X11/Wayland UI suites retain modern snapshot/transfer coverage.
The lifecycle overlay now shows the actual attachment error when service status
reports a running shell, rather than obscuring it with “Shell is running.” Owner
acceptance of the Linux saved-session fix remains pending.


## Staged preview outside the checkout

`just toolkit-stage` builds and copies the current Qt candidate, runtime and
Python service/helpers into a fresh directory. Its launcher resolves the payload
relative to itself; agent wrappers and the invocation-local Copilot plugin also
survive relocation before first use. User preferences retain their existing
`~/.config` location on both platforms. Sessions and window views use a separate
`~/.local/state/terminal-workbench/qt-preview` namespace (with explicit overrides),
and new workspaces default to the user's home rather than the checkout.

`just toolkit-stage-test` exercises relocation, paths containing spaces, all
three agent wrapper argument paths, saved-shell survival across successful and
failed viewer exits, stopped-shell preservation and absence of writes into the
payload. Linux additionally starts the copied real viewer with offscreen/software
Qt and checks that terminal renderers become ready. Existing saved/disposable
launcher tests remain applicable. Native Mac startup and packaging are unverified.

This is a same-machine proof using installed Qt and Python, not a standalone
redistributable or a toolkit adoption decision. Finder launch, desktop integration,
dependency deployment, signing and installed-version session/service handoff remain
open. Do not overwrite or relocate a live payload. See
[staged preview operation and retirement](qt-staged-preview.md).


## Native Mac vertical scrolling acceptance

The owner confirmed on 2026-10-19 that vertical scrolling now works on Mac.
This closes the reported trackpad responsiveness regression; mixed-display
sharpness and unplug behavior remain separate native acceptance checks.


## Preview desktop entry points

The Mac stage now contains a movable `Terminal Workbench Preview.app` with its
Python/runtime/agent payload inside Resources. A small native bootstrap resolves
its own bundle and the Python path recorded at build time, without requiring a
terminal's PATH. The final bundle is signed locally ad hoc. Linux offers explicit
`--install-desktop-entry` after relocation, preserving unrecognized existing
entries. A Distrobox-built preview records the container and routes its host
app-menu entry through `distrobox enter`. Desktop application identity is separate
from the uninstalled UI study.

Desktop launch redirects output to a private append-only `launch.log` in user
state, then execs the viewer with an inherited window-slot lease. Qt removes the
private launch configuration immediately after reading it. Launcher/exec failures
release their lease and temporary config without stopping shells. Desktop launch
itself does not perform the explicit service activation described below. Terminal launches
retain their existing subprocess/cleanup behavior.

The stage smoke starts the real copied Qt viewer in console and desktop modes on
Linux offscreen/software, checking renderer readiness, actual exec replacement,
lease retention and config cleanup. The desktop launcher check compiles the Mac
bootstrap on Linux with only `_NSGetExecutablePath` substituted, exercises a moved
app with a minimal PATH, and checks logs, exec failures, lease release, live shell
preservation and symlink-log refusal. Linux GLib launches cover reserved path
characters and Distrobox argv routing; desktop-file validation and unmanaged-file
protection pass. This does not prove native Finder, codesigning or physical app
menu behavior. Those reviews, bundled dependencies and installed-version lifecycle
remain open. See [preview launch and review steps](qt-staged-preview.md).


## Explicit staged-preview activation

The installed launcher now supports readonly `--status` and explicit `--activate`.
Activation verifies the selected payload, reserves all 64 client window slots,
refuses any active window and verifies an existing staged service's initial folder.
All saved Qt launches acquire their lease before service initialization, so blocked
launches cannot initialize another service configuration during this handoff.
Older launchers also see the reserved slots. The generic session API adds optional
health `service_instance` and advertised `service.retire(expected_instance)`;
a stale observer cannot retire a replacement authority. Unsupported services are
refused without falling back to unconditional shutdown.

Activation starts the selected service through health only: no catalog seed,
journal completion, terminal recovery or ownership change. Existing terminals
retain processes and helper paths; new terminals use the selected payload. Keep
old payloads until their retained shells have exited. Replacement startup failure
leaves shell/runtime processes untouched and releases window reservations for a
retry. This is an explicit client operation, not automatic app-update policy.

The real-PTY activation check verifies unchanged shell PIDs, variables, live
attachment and socket identities, catalog bytes/mtime, view preferences and managed
config; stopped entries stay stopped. It covers repeated activation, stale/missing
instance requests, active-window exclusion, new-helper paths, failed replacement
startup with retry, legacy-method refusal and unrelated-service isolation. Lease
checks cover partial/full rollback and unsafe lock files. Service regressions also
pass. Native Mac activation acceptance and old-payload lifecycle remain open.


## Native Mac staged launch acceptance

On 2026-10-19 the owner confirmed staging and launch work after the read-only
Info.plist fix and retiring the older service configuration once. This confirms
the tested native app launch path; native service activation, dependency deployment
and mixed-display quality remain separate checks.


## Mac desktop shell startup

The owner reported pyenv/atuin/zoxide missing from Finder-launched staged shells.
The runtime started a non-login shell and fell back directly to `/bin/sh` when
`SHELL` was absent. Default Mac startup now honors a usable `SHELL`, otherwise
uses the PTY library's existing account-shell lookup, then falls back to
`/bin/sh`. Recognized default shells receive `-l`, and the resolved default is
exported as `SHELL` for child programs. Explicit runtime commands and Linux default
startup retain their existing modes. Startup files are interpreted only by the
real shell; Workbench does not source them in a helper or modify user dotfiles.

The real-zsh PTY check isolates HOME/ZDOTDIR, starts with `/usr/bin:/bin`, and
shows profile-provided tools missing in non-login mode and available to the
interactive rc in login mode. It verifies startup order, interactive/login options,
TERM_PROGRAM and stable PID after detach/reconnect without rerunning startup.
The default branch is platform-sensitive; Linux explicitly tests login through
`--login-shell`, while a native Mac run tests the automatic default. Native Finder
review remains pending. Retained shells need no restart during activation, but
this environment correction applies to newly created shells.


## Optional Mac Qt deployment preview

`just toolkit-bundle` adds native-Mac `macdeployqt` staging with matching Qt
tools, an explicit QML import manifest, static Mach-O Qt linkage checks and
notice collection before the final ad hoc signature. Python and reported
non-Qt libraries remain external. Ordinary Linux/Mac staging is unchanged.

On 2026-10-19 the owner confirmed the bundle command works on Mac at `164264b`.
That confirms native deployment, configuration generation, the linkage audit
and final signing complete. Finder launch of the Qt-deployed app after moving
it, actual loaded-library paths and QML workflow acceptance remain separate
checks. See [bundle review instructions](qt-staged-preview.md#mac-optional-qt-deployment).

The preceding native retries identified three packaging defects now addressed:
forced overwrite caused repeated library deployment, the tool did not generate
`qt.conf`, and the audit mixed dylib identity names with actual imports.
Deployment now reuses copied libraries, has a fifteen-minute deadline with
progress/process evidence, writes relative Qt configuration before signing,
and audits typed load commands. Native deployment completed in about 75–78
seconds before the final audit corrections; deployment size/time optimization
remains open. Portable tests cover missing/copied configuration, relocation,
actual external imports versus install IDs, and timeout/cancellation cleanup.
No release readiness or production frontend decision is implied by this preview.


## Preview payload references across activation

Runtime capability replies now optionally include versioned launch origin:
the executable path and initial agent-helper directory, captured once at startup.
The installed launcher's readonly `--status` groups those facts by staged
payload, alongside selected-preview and active-service references. Stopped
terminals add no running references; legacy, unavailable or incomplete origins
remain explicit unknowns. No directory discovery, removal or deletion-safety
claim is made: other namespaces and child-program dependencies remain outside
this inventory. Existing clients can ignore the additive capability/JSON fields.

Origin capture runs once per runtime and capability replies reuse the encoded
value. There is no new PTY parsing, rendering or process-tree polling work.
The existing service health cache carries the optional origin through handoff.
Linux `just check` and `just toolkit-activation-test` pass, including retained
versus new build references with unchanged shell PIDs and saved state. Native Mac
origin reporting remains pending. See [preview status and activation](qt-staged-preview.md).


## Mac bundle profiling and deployment overhead

The next packaging slice discovers supported `macdeployqt` options and disables
its intermediate signing pass when `-no-codesign` is advertised. Workbench's
final ad hoc signature remains after configuration, notices and reporting.
The `qmltooling` debugging plugin type is excluded when the tool supports it;
older versions retain their defaults. Broader Qt Quick/Controls style trimming
requires further dependency analysis and is not implemented in this slice.

Printed phase timings and sibling `bundle-metrics.json` record packaging time,
selected switches and regular-file bytes/counts by area. Metrics publication
occurs outside the already signed app. Portable fixtures cover old/new tool
options, signature-order immutability, accounting and symlink exclusion.
The owner measured about 50 seconds for Qt deployment after this change,
compared with a preceding 65-second run; these are single-run observations.
The owner also confirmed Mac runtime/helper origin reporting: the new terminal
appears in both build-reference lists while four older runtimes stay unknown.
See [packaging measurement](qt-staged-preview.md#packaging-size-and-timing).


## Selective QML deployment trial

`just toolkit-bundle-selective` now offers an opt-in Mac packaging comparison.
Qt's CMake-generated import deployment copies scanner-selected module metadata
and plugins into a private prefix before library deployment. The dynamic Basic
style is included in the target scan. Declared Controls style dependencies are
retained; unrelated nested modules are not copied recursively. The default
bundle recipe remains available. Existing configuration, linkage auditing,
notice preservation and final signing still apply.

Portable fixtures cover plugin relocation, relative links, external-link
rejection, private-prefix cleanup and inherited DESTDIR isolation. On Linux,
Qt's actual deployment API copied 26 modules, including Basic and dialogs while
omitting Particles, LocalStorage and XmlListModel. A real Qt engine instantiated
controls, layouts and dialog components with only the copied filesystem QML
imports and resource imports. It still used installed Qt libraries; this does
not prove native Mac loader behavior or native dialog opening.

Native Mac size/time comparison and moved-app launch review remain pending.
Metrics distinguish QML import copying from subsequent library deployment.
See [the trial instructions](qt-staged-preview.md#mac-selective-qml-deployment-trial).


## Pane chrome focus

Clicks on pane titles, unused header space and the outer frame now select the
pane and focus its terminal reader. Terminal selection and the layout button
retain their own pointer handlers; Overview retains its existing click/drag
behavior. The UI regression checks chrome focus and subsequent typing without
changing PTY input during clicks or resizing terminal grids.

The owner also confirmed the selective QML bundle trial works on Mac at
`5a6c9f4`, apart from this pane-chrome focus issue. A native selective-deployment
timing/size comparison has not yet been reported.


## Installed payload verification

The installed launcher's `--check` now exercises runtime startup through its
no-argument usage path and QML startup through a dedicated viewer action,
without constructing UiSession or starting/attaching terminals. The shared
QML probe instantiates Basic controls, layouts and dialog components. The
viewer returns version, search paths and a loaded-image snapshot using isolated
Mac dyld/Linux proc adapters. This code runs once on request, outside terminal
parsing, rendering and regular startup.

For Qt-bundled Mac previews, filesystem QML imports are limited to the copied
tree, observed Qt framework/plugin paths must remain inside the app, and the
existing deep/strict ad hoc signature is verified. Non-Qt image paths are
reported without claiming they are bundled. The checker does not sign files,
select a preview or request the session service. Metadata, startup, signature
and containment failures produce separate results and a nonzero exit.

Portable failure fixtures cover malformed metadata, failed subprocesses,
external Qt/plugins, escaping QML symlinks, missing loader evidence and invalid
signatures. Real Linux relocated-preview coverage checks a running service and
a clean state namespace. The QML export/load proof still passes after sharing
the diagnostic component loader. Native Mac check-command acceptance remains
pending; dialog opening, physical rendering and external Python support remain
separate acceptance items.

The owner confirmed pane-chrome focus on Mac at `f820f0f`; the preceding
selective-bundle workflow acceptance remains recorded above.


## Candidate checks before explicit service activation

`--activate` now runs the installed payload check before creating session state,
reserving window slots or retiring an existing service. A failed runtime/QML
check or Mac signature/Qt-containment check leaves the active service unchanged
and names the failed checks; `--check` retains the full diagnostic report.
Successful activation includes a concise verification summary. These are
client-side candidate checks, without new runtime facts or protocol methods.

Linux `just toolkit-activation-test` passes with a deliberately broken candidate
viewer: rejection preserves the current service instance/PID, terminal sockets
and saved catalog/preferences/views. Rejection in a fresh namespace creates no
session/configuration directory. The same test then activates a working candidate
while retaining shell PIDs, variables, helper origins, stopped entries and saved
state, and still exercises window exclusion, stale retirement and legacy-service
refusal. Later session-service startup failure remains a separate outcome;
preflight does not claim automatic rollback. Native Mac activation review of
these checks remains pending.


## Private service startup rehearsal before handoff

Activation now rehearses the candidate Python session service after the
runtime/QML checks and before creating user session state or retiring an active
service. The rehearsal uses a fresh short mode-0700 directory under `/tmp`,
validates a zero-terminal health reply with the expected process/runtime/cwd,
checks that no catalog or terminal sockets were seeded, and retires the exact
temporary service instance. It does not inspect or attach existing shells.

Startup has a five-second deadline and uses explicit short socket timeouts;
existing callers retain their 60-second default. Cleanup owns only the spawned
process group and its temporary directory, including a lingering descendant
when its parent exits. Failure keeps a private diagnostic log; success removes
it. `--check` retains its no-service behavior. Activation's verification summary
now includes `service_startup` with zero terminals.

Linux activation coverage rejects a candidate whose service exits during
startup while preserving the active service instance/PID and running shells.
Dedicated failure tests cover early exit/logging, startup timeout, an
unresponsive socket, an incomplete dripping reply and interruption with a
SIGTERM-ignoring descendant. This
reduces failures discovered only after retirement; it does not guarantee the
subsequent real-namespace startup or implement automatic rollback. Native Mac
startup-rehearsal acceptance remains pending.
