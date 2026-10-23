# Band

A terminal with scrolling workspaces, in the style of the niri window manager,
for macOS and for Linux desktops that don't run niri.

Pre-release, working toward v0.1: a Qt Quick client in front of a Rust runtime
that owns the shells. (Formerly Terminal Workbench; the repository keeps that
name for now.)

## What it is

- Workspaces of terminal panes laid out side by side in a horizontally scrolling
  strip. Panes keep their width; new ones extend the strip instead of shrinking
  the others.
- Sessions that outlive windows. A background runtime owns every shell, so
  closing or crashing the window detaches, and reopening reconnects.
- An overview of every terminal across workspaces, with search.
- A fresh, configurable keyboard model that follows each platform's conventions
  and routes input explicitly to the terminal.
- A restrained interface inspired by Foot, Ghostty-compatible themes, and
  per-workspace and per-terminal colours.
- Status for coding agents launched through it (working, needs input, ready),
  with navigation to the terminal that is waiting.

## Where it is going

- **Apps:** start TUIs such as lazygit or btop from the command palette and keep
  them in a side panel next to the strip.
- **SSH:** remote panes that remember their host and reconnect.
- **Git worktrees:** open a workspace on a new or existing worktree.
- Possibly, later, deeper support for running agents.

See the [roadmap](docs/roadmap.md) for the plan and the
[status log](docs/status-log.md) for the development record.

This project follows an experiment built on Herdr. It has independent Git history
and contains no copied Herdr code.

## How it fits together

- `band-session` (Rust) runs one process per terminal that owns its PTY and
  Ghostty VT state, plus a session service that keeps the workspace catalog.
  Shells keep running when windows close.
- The Qt client renders terminals and the workspace UI. It attaches to runtimes
  through snapshots and never owns a PTY.
- Optional agent launchers (`band-codex`, `band-claude`, `band-copilot`, `band-pi`)
  report lifecycle hooks, so panes show **Working**, **Needs input** or
  **Ready**. They are symlinks to `band-session`; no Python is needed.

See [architecture](docs/architecture.md) for ownership boundaries.

## Run it

On this Linux workstation, from the checkout:

```sh
distrobox enter build-machine -- just toolkit-workbench
```

This builds the runtime and client and opens your saved development workspaces
from `.local/sessions`. Close detaches; reopening reconnects to the same shells.
To build an installable release (`Band.app` or a Linux archive), run
`just package` and follow [the packaging guide](docs/packaging.md).

On a Mac, follow [the native build guide](docs/macos-build.md).

Inside the window: Commands lists every action, and shortcut help shows the
current bindings ([shortcuts](docs/shortcuts.md)). Overview shows terminals across
workspaces. Appearance sets a workspace palette, and a pane's menu can give one
terminal its own colors. Portable settings live in a dotfile outside the
checkout: `just config-path` finds it, and `just config-init` writes an example
([dotfiles](docs/dotfiles.md)).

## Layout

```text
crates/runtime/      Rust runtime, session service, window launcher and agent hooks
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
Band code uses the [MIT license](LICENSE). Bundled fonts and themes keep
their own licenses beside them in `fonts/` and `themes/`.
