# Native Wayland spike

Linux can now build an opt-in Wayland-only viewer with pinned Raylib/GLFW.
The existing `just spike` stays X11 (XWayland on a Wayland desktop); Mac stays
on Cocoa. This is platform evidence for the toolkit decision.

## Try it on a Wayland desktop

Inside build-machine on this workstation:

```sh
just wayland-spike
```

Run it from the repository in the Distrobox, as with `just spike`. Both commands
use the same saved sessions and ownership rules. Close an owning window first,
or use Move here in the new window. Closing the viewer keeps its shells alive.

The separate binary is `.local/build/viewer-wayland/ghostling`; the standard
binary remains `.local/build/viewer/ghostling`. The Wayland build disables GLFW's
X11 backend, so it cannot silently fall back to XWayland. The launcher requires
Linux and WAYLAND_DISPLAY. Check `.local/sessions/viewer.log` for
`PLATFORM: DESKTOP (GLFW - Wayland): Initialized successfully`.

Beyond the existing build prerequisites, Fedora needs
`wayland-devel wayland-protocols-devel libxkbcommon-devel mesa-libEGL-devel`
in the build environment. Git dependencies still use SSH. Window placement and
offscreen rescue defer to the compositor; absolute desktop coordinates are
unavailable on Wayland. Framebuffer callbacks still drive backing size/scale,
and compositor size changes still reach the PTY.

Check physical typing, held-key repeat, Ctrl-C, copy/selection and Unicode paste;
Super+Alt navigation and Overview; mouse resizing and two-finger scrolling;
detach/reattach and transfers between windows. Compare text sharpness and borders
at native/fractional scale, then move between displays and disconnect a display.
IME/dead-key composition and desktop shortcut conflicts need separate review.
Keep X11 as the fallback while these checks are outstanding.

## Automated evidence

`just wayland-gui-test` starts a private Xvfb server and a nested Niri compositor
with its own configuration and Wayland/IPC sockets. It never uses the caller's
desktop endpoints or starts Niri with `--session`. The Workbench viewer has
DISPLAY removed. The test creates one disposable runtime, raw PTY reader and
catalog; it checks exact fast text/Enter, Unicode bracketed clipboard paste,
compositor-driven PTY resize, and close/reopen with the same reader process and
working input. It exercises the Overview shortcut and captures its rendering.
Screenshots/logs are saved under `.local/artifacts/wayland/`.

The test needs Niri, Xvfb, wl-clipboard, a C compiler, pkg-config, wayland-scanner
and the Wayland/xkbcommon development packages. Its small virtual keyboard helper
uploads a standard US keymap and real physical keycodes. wtype's dynamically
assigned keycodes are unsuitable for this GLFW physical-key test. The virtual
keyboard protocol fixture retains its upstream MIT notice (from
[atx/wtype](https://github.com/atx/wtype/blob/master/protocol/virtual-keyboard-unstable-v1.xml)).
The helper connects only to the private compositor selected by the test.

Validated on 2026-10-15 with Niri 26.04 and Mesa llvmpipe. The nested compositor
follows Niri's [development testing approach](https://github.com/niri-wm/niri/wiki/Developing-niri).
This proves native Wayland client integration under software rendering; it does
not prove physical GPU performance, input methods, trackpad feel, physical
mixed-DPI display transitions or other compositors. Those remain acceptance
gates before making Wayland the default or choosing the final toolkit.

The owner confirmed that the opt-in Wayland spike works on their Linux desktop
on 2026-10-15. This is a general desktop smoke confirmation; detailed physical
input, selection/copy, trackpad, GPU performance, input-method and display-scaling
acceptance remains separate.

Keyboard-layout validation now has a separate `just keyboard-layout-gui-test`
recipe using the same private compositor. It covers German/French modified keys,
Kitty reporting and a US-international dead-key commit. See
[layout behavior and limits](terminal-input.md#keyboard-layouts-and-composed-text).

## Display-scale transitions

`just wayland-display-gui-test` changes the **private nested compositor's** output
scale through 1 → 1.25 → 1.5 → 2 → 1. It needs xdotool in addition to the tools
above, solely to size the nested compositor's window on its private Xvfb display.
The Workbench viewer still runs Wayland-only with DISPLAY unset. The user's
output configuration, scale and keyboard layout are untouched.

A fixed logical floating window keeps density changes independent of estate
changes. The test requires actual framebuffer-driven backing-scale updates,
checks captured pixel dimensions against each scale (allowing decoration rounding),
and verifies that logical window size and the PTY grid remain constant. Scale
changes occur with Overview open, exercising its cached surfaces; captures include
both Overview and the working view. Text after each transition must arrive exactly
once in the same shell-side reader process. The existing clipboard, compositor
resize and close/reopen checks follow.

Screenshots, logs and a structured `display.json` report are retained under
`.local/artifacts/wayland-display/`. The clipboard readiness probe tolerates the
brief interval before its disposable owner publishes an offer; screenshot checks
wait for Niri's asynchronous file creation.

This is a reproducible compositor integration check under llvmpipe. Physical
monitor movement/unplug, GPU performance, trackpad gestures and the Mac external
screen comparison still require native review. Pixel dimensions and a successful
atlas refresh alone do not establish text sharpness or visual acceptance.

The owner confirmed that the full display-scale suite passed on their machine
on 2026-10-15. This records the automated nested-compositor check; physical
monitor and native Mac display review remain separate.

## Pointer selection and clipboard

`just wayland-pointer-gui-test` exercises forward/reverse drag selection,
Unicode copy/paste through the real compositor clipboard, SGR mouse reports and
Shift-forced local selection. It runs at 1×, after moving to 1.5×, and after
closing/reopening the viewer at 1.5×, retaining the same reader process. Explicit
clipboard sentinels ensure a stale previous copy cannot pass the assertion.

The private Niri fixture disables client decorations to make pointer coordinates
unambiguous and reads its own window positions after compositor scale changes.
xdotool supplies pointer motion/buttons to the private parent X server; Niri
forwards native Wayland events to the viewer, which has DISPLAY unset. Shift is
held through the private test keyboard. Captures/logs are retained under
`.local/artifacts/wayland-pointer/`. This covers the desktop integration path;
physical trackpads, other compositors and native Mac selection still require review.

The check exposed two native adapter problems:

- Pinned GLFW 3.4 ignored the coordinates carried by Wayland pointer enter.
  A first click after entry could use the old position. The marked input patch
  now publishes the entry coordinates; disabled/relative cursor mode keeps its
  existing position-event policy. The compiled callback check also covers foreign
  surfaces, decoration surfaces and null surfaces.
- Raylib initialized mouse scale from logical/framebuffer ratios. GLFW cursor
  coordinates are already relative to the logical window. Startup at fractional
  density therefore applied a second scale and displaced hit testing. The marked
  display patch now retains identity mouse scaling. Restoring only the old scaling
  reproduced a failed first selection after reopening at 1.5×, while the preceding
  1× and dynamic 1.5× cases passed.

Both patches preserve upstream Raylib/GLFW zlib notices and remain idempotent.
Remove the Wayland enter adaptation when the pinned upstream backend publishes
entry coordinates itself; retain the regression when replacing the toolkit.
Keep input coordinates and render backing pixels independent in future adapters.
