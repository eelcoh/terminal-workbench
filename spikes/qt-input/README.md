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
