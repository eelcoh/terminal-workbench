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
build creates an application bundle. Native Mac execution has not been verified.
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
also has a Mac app-bundle launch path, which still needs native validation.

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
complete graphemes per cell but does not shape across cells. Terminal semantic
accessibility, app mouse reporting, images, full query callbacks, selection
word/autoscroll tracking, multi-pane performance and display acceptance remain
open. QPainter's image-to-texture upload also needs profiling before adoption.
