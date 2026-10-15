# Production toolkit readiness

Status: 2026-10-15. Keep the approved minimal chrome and Overview interaction.
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
