# Roadmap

Terminal Workbench is a desktop terminal workspace for running and overseeing
coding agents on macOS and Linux. The history of every slice so far is in the
[status log](status-log.md); this page is the plan.

## Decisions (2026-10-22)

- **Qt Quick is the production client.** It renders better than the Raylib viewer
  on Mac and on external displays. The Raylib/Ghostling viewer (`just spike`) is
  frozen as a reference: no new features, removal once Qt reaches parity.
- **Shipped runtime code is binaries only.** Users must not need Python. Python
  may remain for development, tests and build tooling.
- **New non-UI code is Rust.** The Qt client stays C++/QML and should stay thin:
  presentation and input, with runtime facts coming from the Rust side.
- **Release numbering:** v0.1 daily driver, v0.2 agent oversight, v0.3 reach.

## Now: consolidate

- [x] Record the toolkit decision and freeze the Raylib viewer.
- [x] Replace the prose roadmap with this page.
- [ ] Choose the product name (owner). Needed before packaging identifiers.
- [ ] Define the v0.1 Qt parity list against `just spike` and close the gaps.

## v0.1: daily driver on both platforms

Goal: the owner uses Workbench as their only terminal for a week on Linux and Mac,
installed from a release artifact on a machine without Python.

1. **One Rust daemon.** Move the session authority into `workbench-session`:
   - [x] `workbench-session service` replaces the Python server: catalog,
     creation, close, move, recovery, lifecycle, health and shutdown, still on
     protocol 1. The Qt client is unchanged, and the Python tests check the Rust
     server over its socket.
   - [ ] Launch clients (`session_service.request`, the management CLI) still
     need Python. They move to the Rust binary and native launch in step 3.
   - `session_shutdown.py`, `terminal_lifecycle.py`, `window_slots.py`,
     `config_paths.py`: subcommands or library code in the same binary.
2. **Binary agent integration.** `agent-hook.py`, `agent_status.py`,
   `agent_watch.py`, `workbench-agent.py` and the generated `wb-*` launchers
   become `workbench-session` subcommands, so agent hooks run without `python3`.
3. **Native launch.** Replace `toolkit-ui.py` and the Mac `preview_launcher.c`
   bootstrap with startup in the Qt executable (service start, window slot lease,
   saved-view config). Replace `platform_dialogs` with Qt's native folder dialog.
4. **Qt parity and spike removal.** Close the parity list, then delete
   `spikes/ghostling`, its patch scripts and spike-only tests.
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
