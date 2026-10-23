# Wayland

The Qt client runs natively on Wayland through Qt's Wayland platform plugin
(`qt6-qtwayland` on Fedora) and falls back to X11 where Qt selects it. Mac is
unaffected.

## Automated evidence

These recipes start a private Xvfb server and a nested Niri compositor; they never
use the current desktop session:

```sh
just toolkit-terminal-wayland-test   # fractional scale, input, selection, clipboard
just toolkit-ui-test                 # the full UI smoke on X11 and then Wayland
just toolkit-search-gui-test         # focused terminal search on both backends
```

They require niri, Xvfb, wl-clipboard and the Wayland/xkbcommon development
headers in the `build-machine` Distrobox. The virtual keyboard helper is
`tests/wayland_keyboard.c`.

The earlier Raylib/GLFW Wayland build and its display-scale and pointer checks
were removed with that viewer; the [status log](status-log.md) keeps its
findings.
