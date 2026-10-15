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
