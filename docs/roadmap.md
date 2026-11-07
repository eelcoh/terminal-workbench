# Roadmap

Band is a terminal with niri-style scrolling workspaces for macOS
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
  belt, after the scrolling strip. Renamed throughout with packaging (step 5):
  app ID `io.github.eelcoh.Band`, `band-*` binaries, `BAND_*` variables and
  `~/.config/band` / `~/.local/state/band`, migrated from `terminal-workbench`.
- [x] Define the v0.1 Qt parity list against `just spike` (step 4 below).

## v0.1: daily driver on both platforms

Goal: the owner uses Band as their only terminal for a week on Linux and Mac,
installed from a release artifact on a machine without Python.

1. **One Rust daemon.** Move the session authority into `band-session`:
   - [x] `band-session service` replaces the Python server: catalog,
     creation, close, move, recovery, lifecycle, health and shutdown, still on
     protocol 1. The Qt client is unchanged, and the Python tests check the Rust
     server over its socket.
   - [x] Opening a window starts the service from the Rust binary (step 3).
   - [x] The Python copies are retired (2026-10-23): the old Python server and
     catalog, `session_shutdown.py`, `terminal_lifecycle.py` and
     `config_paths.py` are deleted. Their catalog, lifecycle and preference
     checks are Rust unit tests; the crash-journal, shutdown and recovery tests
     drive the Rust service. `band-session config-path [--prepare | --init]`
     backs `just config-path` and `just config-init`. `window_slots.py` and the
     development launcher `toolkit-ui.py` remain as development tooling.
2. [x] **Binary agent integration.** `band-codex`, `band-claude`, `band-copilot`,
   `band-pi` and `band-agent-hook` are symlinks to `band-session`, which picks
   the launcher or hook observer from the name it is invoked through
   (`band-session agent …` / `agent-hook …` also work). Agent hooks no longer
   need `python3`. `agent_status.py` and `agent_watch.py` remain developer
   diagnostics behind `just agent-status` / `just agent-watch`.
3. [x] **Native launch.** `band-session open` opens installed windows:
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
5. [x] **Packaging** (2026-10-23; `just package` confirmed on Mac 2026-10-24).
   `just package` builds a zipped, ad-hoc signed `Band.app` with Qt and the
   runtime inside it, or a Linux tar.gz using the system Qt with
   `band --install-desktop-entry` (optionally through Distrobox). The rename to Band landed with it. Preview
   staging, activation and inventory tooling are removed. Flatpak or AppImage,
   an app icon, signing and notarization come later. See [packaging](packaging.md).
6. [x] **macOS CI** (first green run 2026-10-31). `.github/workflows/macos.yml`
   (checkout and the Ghostty clone use the `SSH_PRIVATE_KEY` secret): `just check`,
   offscreen Qt client tests and `just package` on Apple Silicon, with `Band.app`
   as a downloadable artifact. Every check runs after a failure, and failure logs
   are kept as an artifact. `.github/workflows/linux.yml` (2026-10-31) does the
   same in a Fedora 44 container as an unprivileged user and uploads the tested
   Linux archive.
   The owner opened the CI-built `Band.app` on macOS 26 (2026-11-01). A browser
   download is quarantined: Gatekeeper refuses the ad-hoc signed app, and a
   translocated copy failed with a bare `Invalid argument`. The launcher now
   explains translocation, launch errors name their stage and path, and the
   bundle checks itself on its own Qt plugins (496 files instead of 706 after
   pruning unused Qt Quick styles). `just package-install` and `just install-ci`
   install without quarantine ([packaging](packaging.md#install)). A Mac reboot
   left every shell stopped, as designed; Commands → **Start all stopped shells**
   restarts them in one step.
   Installed windows now run the service, runtimes and agent launchers from a
   Band-owned copy of each build, so replacing or deleting the install is safe
   for running shells and a new build takes over the service
   ([install updates](install-updates.md)). Tested on Linux; a native Mac run
   from the copy is pending.
   The manual checklist for input, IME, displays and trackpad stays in
   [the macOS build guide](macos-build.md#native-checks).
7. [x] **Eight-by-eight ceiling removed** (2026-10-25). A window holds up to
   64 workspaces and 256 terminals in any split; moves no longer check room. The
   bounds keep a worst-case catalog and health reply inside the service message
   limit, now 1 MiB on every side. Position markers in the top bar show a window
   around the selected terminal when they do not all fit. The limits are unit
   tested on catalog data; the UI test runs fifteen live terminals and a ninth in
   one workspace. The marker window and a window with hundreds of live terminals
   have not been exercised yet.

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
- **App definitions** in the dotfile: name, command and a working directory
  that defaults to the workspace folder.
- **Apps are strip panes** (revised 2026-11-06; [apps](apps.md)). No side
  panel or dock: an app is an ordinary pane in its workspace. What makes it
  first-class is **Add applications**, a catalog of TUIs (curated, plus
  awesome-tuis entries Homebrew installs) with installed checks, installs in a
  visible pane through Homebrew, the system package manager or COPR, and a
  Sources view that can set up Homebrew.
- **Two ways to navigate** (owner, 2026-11-07): typed lists across workspaces
  (Agents, Apps; Shells if needed) and the Workspaces panel grouped by
  workspace, where only the current workspace lists its panes. Sized for about
  10 panes in a workspace and 50 in all; a flat typed list with search and
  closed workspaces keep 50 readable. Hundreds are not a goal.
- An app pane is an ordinary runtime terminal, so it survives detach and
  restart. When the program exits normally, its pane closes; after a failure
  it stays with its last lines.
- Running apps belong to their workspace (owner, 2026-11-05).

## Polish (after v0.2; notes 2026-11-07)

Rex shows how far polish can carry this kind of app; Band's edge is apps, the
scrolling strip and Linux. [Polish notes](polish.md) list what to learn, in
order: pane identity (kind icon, process and folder), key chips, one accent
and selection style, actions on the focused pane, a first-run flow in a pane
(with picking apps), a sidebar filter, and window chrome last. A first pass of
items 1–7 shipped on 2026-11-07 (the integrated title bar opt-in). The owner's
verdict: nicer, but not half as polished and subtle as Rex; it needs a real
designer's eye, planned as a design step in v0.4.

## v0.3: SSH hosts and worktrees

- **Hosts panel.** Saved SSH hosts. A host pane is a runtime terminal running
  `ssh`. *Done (2026-11-07/08):* `[host.ID]` and New host… (Band's own list,
  `~/.ssh/config` suggestions), Connect, the Hosts panel with open panes and
  reachability; host panes come back stopped after a restart and connect
  again by hand. [SSH hosts](hosts.md).
- **A colour theme per host.** *Done:* the `theme` field, also in New host…. A host can be pinned to a terminal theme, so every
  pane on that host is recognisable at a glance. It uses the existing
  per-terminal colours; an explicit per-terminal choice still wins.
- **Worktrees** in the Workspaces panel: create or pick a Git worktree and open a
  workspace on it, with the worktree shown per workspace. *Done (2026-11-07):* in New
  workspace, a folder inside a repository lists its worktrees and takes a branch
  for a new one (`git worktree add`, beside the main checkout); the workspace
  row shows a linked worktree's branch ([workspace
  folders](workspace-directories.md)).
- **Uncommitted changes.** Mark a pane's caption and its workspace in the
  Workspaces panel when the working tree has changes that are not committed.
  Today the runtime reads only small Git metadata files (HEAD, gitfile,
  commondir) and never starts `git`. Dirty state needs either a rate-limited,
  off-thread `git status --porcelain` per repository (shared by panes in the
  same worktree, run on directory change and after commands) or reading the
  index itself; the first is the likely choice. *Done (2026-11-07):* the context worker
  runs `git status --porcelain` with fsmonitor off, a two-second limit and
  backoff, published through a CTX2 subscription; an accent dot marks pane
  headers, sidebar pane rows and workspace rows ([terminal
  context](terminal-context.md)). Not yet shared between panes of one worktree
  or triggered after commands.
- A remote runtime (the daemon on the remote machine, the client local) is
  evaluated after plain SSH panes. With it, a host would hold its own
  workspaces, and the host level in the sidebar is reconsidered then (owner,
  2026-11-07; [SSH hosts](hosts.md)).

## v0.4: a design step

A designer's pass over the whole client rather than more engineering polish
(owner, 2026-11-07): type scale, spacing and density, color and contrast per
theme, icons, motion, the window chrome on both platforms, and the welcome.
Starts from the first polish pass ([polish notes](polish.md)) and Rex as the
reference for subtlety; the result is a small design system the QML follows.

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
