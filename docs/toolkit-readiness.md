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
