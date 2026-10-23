# Terminal Workbench

A desktop terminal workspace for running and overseeing coding agents.

Working name. Pre-release, working toward v0.1: a Qt Quick client in front of a
Rust runtime that owns the shells.

## Product direction

- macOS and Linux are first-class targets.
- A restrained interface inspired by Foot, with visual and interaction quality approaching Dank Linux.
- Ghostty-compatible terminal themes with matching application colors.
- Workspaces, horizontally scrolling terminal panes, and an overview of agents across workspaces.
- Clear indicators for agents working or awaiting input, with direct navigation to their terminal.
- Persistent sessions owned by a background runtime, independent of open windows.
- A fresh, configurable keyboard model with platform conventions and explicit terminal input routing.

See the [roadmap](docs/roadmap.md) for the plan to v0.3 and the
[status log](docs/status-log.md) for the development record.

This project follows an experiment built on Herdr. It has independent Git history
and contains no copied Herdr code.

## How it fits together

- `workbench-session` (Rust) runs one process per terminal that owns its PTY and
  Ghostty VT state, plus a session service that keeps the workspace catalog.
  Shells keep running when windows close.
- The Qt client renders terminals and the workspace UI. It attaches to runtimes
  through snapshots and never owns a PTY.
- `wb-codex`, `wb-claude`, `wb-copilot` and `wb-pi` launch agents with lifecycle
  hooks, so panes show **Working**, **Needs input** or **Ready**. They are
  symlinks to `workbench-session`; no Python is needed.

See [architecture](docs/architecture.md) for ownership boundaries.

## Run it

On this Linux workstation, from the checkout:

```sh
distrobox enter build-machine -- just toolkit-workbench
```

This builds the runtime and client and opens your saved development workspaces
from `.local/sessions`. Close detaches; reopening reconnects to the same shells.
To install a preview outside the checkout, run `just toolkit-install` and follow
[the staged preview guide](docs/qt-staged-preview.md).

On a Mac, follow [the native build guide](docs/macos-build.md).

Inside the window: Commands lists every action, and shortcut help shows the
current bindings ([shortcuts](docs/shortcuts.md)). Overview shows terminals across
workspaces. Appearance sets a workspace palette, and a pane's menu can give one
terminal its own colors. Portable settings live in a dotfile outside the
checkout: `just config-path` finds it, and `just config-init` writes an example
([dotfiles](docs/dotfiles.md)).

## Layout

```text
crates/runtime/      Rust runtime, session service, launcher and agent hooks
spikes/qt-input/     Qt Quick client (C++/QML); core/ holds its pure C contracts
native/              macOS and Linux platform adapters
scripts/             Build, staging, preview management and diagnostics (Python)
tests/               Real PTY, service and isolated graphical checks
docs/                Design notes, platform guides and the roadmap
```

## Development

Use stable Rust, `just`, Python 3.11+, Git, CMake, Ninja, a C/C++ compiler,
Qt 6.8+ and Zig 0.16.0. The shell-startup checks also need zsh. On this
workstation those tools live in the `build-machine` Distrobox, and Rust is
intentionally absent from the host.

`just runtime-build` prepares the pinned libghostty-vt and builds the runtime.
It finds Zig 0.16.0 in `.local/tools` or PATH; `ZIG=/absolute/path/to/zig`
selects another. Dependency fetches use SSH.

```sh
distrobox enter build-machine -- just check
distrobox enter build-machine -- just toolkit-ui-test
distrobox enter build-machine -- just toolkit-terminal-test
distrobox enter build-machine -- just toolkit-terminal-wayland-test
```

`just check` runs Rust formatting, Clippy and unit tests, real PTY, service and
agent-hook checks, and the client's C core contracts. The graphical tests need
Linux, Xvfb, xdotool and xclip, and for Wayland niri and wl-clipboard; they never
touch the running desktop session.

Source is hosted at [eelcoh/terminal-workbench](https://github.com/eelcoh/terminal-workbench).
Workbench code uses the [MIT license](LICENSE). Bundled fonts and themes keep
their own licenses beside them in `fonts/` and `themes/`.
