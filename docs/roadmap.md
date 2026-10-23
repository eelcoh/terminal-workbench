# Roadmap

Terminal Workbench is a desktop terminal workspace for running and overseeing
coding agents on macOS and Linux. The history of every slice so far is in the
[status log](status-log.md); this page is the plan.

## Decisions (2026-10-22)

- **Qt Quick is the production client.** It renders better than the Raylib viewer
  on Mac and on external displays. The Raylib/Ghostling viewer was removed once
  Qt reached parity (2026-10-23).
- **Shipped runtime code is binaries only.** Users must not need Python. Python
  may remain for development, tests and build tooling.
- **New non-UI code is Rust.** The Qt client stays C++/QML and should stay thin:
  presentation and input, with runtime facts coming from the Rust side.
- **Release numbering:** v0.1 daily driver, v0.2 agent oversight, v0.3 reach.

## Now: consolidate

- [x] Record the toolkit decision and retire the Raylib viewer.
- [x] Replace the prose roadmap with this page.
- [ ] Choose the product name (owner). Needed before packaging identifiers.
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
   - `session_shutdown.py`, `terminal_lifecycle.py`, `window_slots.py`,
     `config_paths.py`: subcommands or library code in the same binary.
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
   no longer goes through the spike (`scripts/build-runtime.py`). Remaining
   cleanup: the service's TSV projection, which no client reads, and design
   notes that still describe the viewer. Parity gaps closed first (2026-10-22):
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

## v0.2: agent oversight

- Generic signals for agents without hooks: bell, OSC 9/777 notifications,
  title changes and output idleness. Hooks stay the precise layer on top, behind
  a small per-agent adapter interface. Unknown remains the honest fallback.
- Desktop notifications when an agent needs input, and a "next waiting agent" key.
- Time in each state and a short per-agent timeline.
- Worktree-aware work: changed-file counts per worktree, and "new agent in a new
  worktree" as one action.
- Close the open lifecycle edges: Claude cancellation, Copilot exit/cancel/permission.

## v0.3: reach

- Remote runtime over SSH: the daemon on a development machine, the client local.
- App profiles: lazygit, file managers, monitors and agents as named pane types,
  with explicit new-instance versus focus and restart after exit. See
  [the status log](status-log.md#apps-workbench-as-a-tui-window-manager).

## Later

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
