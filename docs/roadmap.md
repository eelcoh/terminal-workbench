# Roadmap

Terminal Workbench is a terminal with niri-style scrolling workspaces for macOS
and for Linux desktops that don't run niri. Agent status is one feature, not the
product's focus (owner, 2026-10-23). The history of every slice so far is in the
[status log](status-log.md); this page is the plan.

## Decisions (2026-10-22)

- **Qt Quick is the production client.** It renders better than the Raylib viewer
  on Mac and on external displays. The Raylib/Ghostling viewer was removed once
  Qt reached parity (2026-10-23).
- **Shipped runtime code is binaries only.** Users must not need Python. Python
  may remain for development, tests and build tooling.
- **New non-UI code is Rust.** The Qt client stays C++/QML and should stay thin:
  presentation and input, with runtime facts coming from the Rust side.
- **Release numbering:** v0.1 daily driver, v0.2 apps and the sidebar, v0.3
  SSH hosts and worktrees (revised 2026-10-23).

## Now: consolidate

- [x] Record the toolkit decision and retire the Raylib viewer.
- [x] Replace the prose roadmap with this page.
- [x] Choose the product name (owner, 2026-10-23): **Band**, Dutch for tape or
  belt, after the scrolling strip. It is hard to search for alone, so public
  identifiers pair it with "terminal" where needed. The rename of the app,
  bundle, desktop entry and state/config directories (with migration from
  `terminal-workbench`) happens with packaging in step 5.
- [x] Define the v0.1 Qt parity list against `just spike` (step 4 below).

## v0.1: daily driver on both platforms

Goal: the owner uses Workbench as their only terminal for a week on Linux and Mac,
installed from a release artifact on a machine without Python.

1. **One Rust daemon.** Move the session authority into `workbench-session`:
   - [x] `workbench-session service` replaces the Python server: catalog,
     creation, close, move, recovery, lifecycle, health and shutdown, still on
     protocol 1. The Qt client is unchanged, and the Python tests check the Rust
     server over its socket.
   - [x] Opening a window starts the service from the Rust binary (step 3).
   - [x] The Python copies are retired (2026-10-23): the old Python server and
     catalog, `session_shutdown.py`, `terminal_lifecycle.py` and
     `config_paths.py` are deleted. Their catalog, lifecycle and preference
     checks are Rust unit tests; the crash-journal, shutdown and recovery tests
     drive the Rust service. `workbench-session config-path [--prepare | --init]`
     backs `just config-path` and `just config-init`. `window_slots.py` and the
     development launcher `toolkit-ui.py` remain with preview tooling until
     packaging (step 5).
2. [x] **Binary agent integration.** `wb-codex`, `wb-claude`, `wb-copilot`,
   `wb-pi` and `wb-agent-hook` are symlinks to `workbench-session`, which picks
   the launcher or hook observer from the name it is invoked through
   (`workbench-session agent …` / `agent-hook …` also work). Agent hooks no longer
   need `python3`. `agent_status.py` and `agent_watch.py` remain developer
   diagnostics behind `just agent-status` / `just agent-watch`.
3. [x] **Native launch.** `workbench-session open` opens installed windows:
   user state and preferences (including the macOS migration), the window-slot
   lease, service startup, the launch config and the desktop log. The payload
   launcher, Linux desktop entry and Mac bootstrap use it. The Qt client already
   uses Qt's folder dialog.
   Preview management (`--check`, `--status`, `--activate`, `--paths`,
   `--install-desktop-entry`) and the development launcher `toolkit-ui.py` still
   use Python; they are reworked with packaging in step 5.
4. [x] **Qt parity and spike removal.** `spikes/ghostling`, its Raylib patches,
   launcher and spike-only tests are deleted (2026-10-23). The C headers the Qt
   client shared with it moved to `spikes/qt-input/core/`, and the runtime build
   no longer goes through the spike (`scripts/build-runtime.py`). The
   viewer-only TSV projections are no longer written. Design notes that still
   describe the viewer remain as history. Parity gaps closed first (2026-10-22):
   - [x] Mouse reporting to terminal applications (press/release/motion/wheel),
     with Shift to select locally.
   - [x] Double-click word and triple-click line selection, using VT gestures.
   - Unseen-output marks on unfocused terminals: not needed (owner, 2026-10-22).
   - Drag a pane edge to resize: not needed; Qt keeps presets, cycle and full
     width (owner, 2026-10-22).
   - [x] Per-terminal colors with "Use workspace" (Mac review passed 2026-10-23).
     They change only that terminal's content; the pane frame keeps the workspace
     palette. A swatch after the title marks the terminal and reopens the picker,
     and workspace Appearance says how many terminals keep their own colors.
   - A terminal scrollbar: not needed (owner, 2026-10-22).
   Selection drags do not autoscroll at the edge.
5. **Packaging.** An unsigned Mac `.app` with Qt and the runtime inside it, and a
   Linux archive with a desktop entry (Flatpak or AppImage evaluated afterwards).
   Simplify preview staging to what these artifacts need.
6. **macOS CI.** Build plus headless tests on a macOS runner; keep a short manual
   checklist for input, IME, displays and trackpad per release.
7. **Remove the eight-workspace ceiling** or document why it stays.

Exit criteria: release artifacts for both platforms, no Python on the user path,
detach/reattach and agent status working from an installed build, and a week of
daily use with no lost sessions.

## v0.2: apps and the sidebar

Goal: TUIs such as lazygit and btop live next to the strip and are one key away.

- **One panel per rail icon.** The drawer's Workspaces/Agents switch goes away.
  Each icon on the left rail owns one panel; clicking it opens that panel, and
  clicking the active icon closes it. Pinning keeps working as now. Fixed icons,
  top to bottom: Workspaces, Apps, Hosts (v0.3) and Agents, which appears only
  once an agent has been seen and keeps its waiting badge.
- **App definitions** in the dotfile: name, command, arguments and a working
  directory that defaults to the workspace folder.
- **Apps panel.** Lists the configured apps; choosing one starts it. A running
  app is shown in the side panel as a live terminal with its own width, beside
  the strip rather than in it.
- **The rail as a dock.** Each running app gets a small icon under the fixed
  ones. Clicking it brings that app into the side panel; clicking again hides it
  while it keeps running. Command-P / Super-P lists apps and does the same.
- An app pane is an ordinary runtime terminal, so it survives detach and
  restart. Its placement and visibility are client state. When the program
  exits, its pane closes instead of dropping to a shell.
- Open: whether running apps belong to a workspace or to the window.

## v0.3: SSH hosts and worktrees

- **Hosts panel.** Saved SSH hosts. A host pane is a runtime terminal running
  `ssh`, remembered so it reconnects after a restart.
- **A colour theme per host.** A host can be pinned to a terminal theme, so every
  pane on that host is recognisable at a glance. It uses the existing
  per-terminal colours; an explicit per-terminal choice still wins.
- **Worktrees** in the Workspaces panel: create or pick a Git worktree and open a
  workspace on it, with the worktree shown per workspace.
- **Uncommitted changes.** Mark a pane's caption and its workspace in the
  Workspaces panel when the working tree has changes that are not committed.
  Today the runtime reads only small Git metadata files (HEAD, gitfile,
  commondir) and never starts `git`. Dirty state needs either a rate-limited,
  off-thread `git status --porcelain` per repository (shared by panes in the
  same worktree, run on directory change and after commands) or reading the
  index itself; the first is the likely choice.
- A remote runtime (the daemon on the remote machine, the client local) is
  evaluated after plain SSH panes.

## Later

- Agent oversight, if it still earns its place: generic signals for agents
  without hooks (bell, OSC 9/777, title changes, idleness) behind a small adapter
  interface, desktop notifications, time in each state, "new agent in a new
  worktree", and the open Claude/Copilot lifecycle edges.
- Plugins with versioned capabilities and crash isolation
  ([notes](status-log.md#plugins-long-horizon)).
- Signing, notarization and updates.
- Complex-script shaping and screen-reader access to terminal content.

## Release gates (all releases)

- Native macOS and Linux (X11 and Wayland) validation, recorded separately.
- No lost sessions across window close, client crash or service restart.
- Real-agent validation with honest fallback states.
- Predictable keyboard routing, including forwarding intercepted chords.
- Configuration outside the install, with malformed settings keeping a usable app.
