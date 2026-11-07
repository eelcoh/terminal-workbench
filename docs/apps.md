# Apps (v0.2 plan)

Plan (2026-11-05, revised 2026-11-06 and 2026-11-07). Slices 1–7 are implemented; see
[app terminals](session-service.md#app-terminals) and
[dotfiles](dotfiles.md#apps). Goal from the [roadmap](roadmap.md#v02-apps-and-the-sidebar):
TUIs such as lazygit and btop are first-class and one key away. Running apps
belong to their workspace (owner, 2026-11-05).

Revision (owner, 2026-11-06): no special pane for apps. An app is a standard
pane in the strip; what makes it first-class is how it is found, installed and
set up, after Omarchy's use of TUIs: a catalog, open when installed, install
(Homebrew or the system package manager) when not, and custom setup. The side
panel shipped in slice 4 and an uncommitted dock were dropped.

Revision (owner, 2026-11-07): the sidebar shows only running apps, with **Add
applications**, which opens a catalog: installed apps on top, then apps to
install. Limited to TUIs, taken from lists such as awesome-tuis. Two sidebar
layouts are on trial (slice 8).

## Model

An app is a runtime terminal like any other, so it survives detach, window
close and service handover. The difference is in the catalog and the client:

- **Catalog.** A terminal entry may carry `"app": {"id", "command",
  "directory"}`, saved when the app starts. It stays in its workspace's
  `terminals` list and is a normal pane in the strip: moves within the
  workspace, widths, Overview and search apply. It cannot move to another
  workspace.
- **Runtime.** The service starts an app's runtime with the command from its
  definition instead of a plain shell. The runtime runs it as
  `$SHELL -l -c '<command>'`, so `PATH` from the login shell (Homebrew,
  `~/.local/bin`) applies and quoting is the shell's. It starts in the
  definition's directory, or the workspace folder by default.
- **Exit.** When the program exits normally, the pane closes. When it exits
  non-zero or by a signal, the runtime leaves its last lines in an `.output`
  file and the pane stays, stopped, with the exit code and those lines,
  **Start again** (the same terminal, `terminal.recover`) and **Close pane**.
  The service removes ended apps on the next change, except failed ones, which
  stay until closed or until a window opens.
- **After a reboot**, stopped apps are dismissed, not restarted. Starting one
  again is one key. **Start all stopped shells** skips and removes them.
- **One per workspace.** Choosing a running app goes to its pane.

## Definitions

In `config.conf`, one section per app; the section name is its ID:

```text
[app.lazygit]
name = Lazygit
command = lazygit

[app.btop]
command = btop --utf-force
directory = ~
```

`command` is required. `name` defaults to the ID. `directory` is absolute, `~`,
or relative to the workspace folder. `[linux.app.NAME]` and `[macos.app.NAME]`
override per platform, as for shortcuts. Unknown keys reject the file, as
today.

## Slices

1. [x] **Runtime and catalog.** `terminal.create` takes an optional app (ID,
   command, directory). The runtime gains a command mode. Exited and
   after-reboot app terminals are removed; recovery and bulk start never
   restart them. At most 64 apps and 1023-byte commands keep the worst-case
   catalog in one message. Rust unit tests and `tests/app_terminal_test.py`.
2. [x] **Definitions.** `[app.*]` in the shared C config parser, so every
   settings reader accepts the file; values reach QML as
   `configuration.apps`, inside the existing all-or-nothing reload. At most
   32 definitions. C and Qt configuration tests.
3. [x] **The rail, one panel per icon.** The drawer's Workspaces/Agents
   switch is gone; clicking an icon opens its panel and clicking the active
   icon closes it (a pinned panel hides without unpinning). The temporary
   panel now opens beside the rail instead of a Drawer over it, so the rail
   stays usable. The Agents icon appears once an agent has been seen.
4. [x] **Apps panel; apps as strip panes.** The Apps icon (shown when apps
   are defined) lists definitions; choosing one starts it as a pane in the
   current workspace, or goes to it if it is running. Show apps (`apps`,
   Super+Alt+A / Command+Option+A; see [shortcuts](shortcuts.md)) opens the
   list. A failed app keeps its pane as above. The UI test covers start, going
   to a running app, the shortcut, a failed start kept and started again, and
   a normal exit.
5. [x] **Catalog.** Built in (`ui_apps.cpp`): lazygit, lazydocker, btop,
   htop, yazi, neovim, k9s, tig, ncdu and gitui, each with a one-line
   description and its package names. `[app.ID]` sections come first and add
   to the list; one with a catalog ID overrides its name, command and folder.
   Opening the Apps panel checks which commands exist: the login shell's
   `PATH` (read with `env`, so any shell works) and `command -v` in `sh`.
   Rows show *Running*, *Install*, *Not installed* or nothing (installed).
6. [x] **Install.** Choosing a missing app runs its recipe as an app pane
   named "Install NAME", so output and prompts (sudo, dnf's y/N) are the
   user's to see and answer. Homebrew first on both platforms; on Linux
   otherwise `sudo dnf install`, `sudo apt install` or `sudo pacman -S` where
   that manager has the package (owner, 2026-11-06). Image-based (ostree)
   Linux hosts get no system recipe. When the install exits normally its pane
   closes, the check runs again and the app opens; a failed install keeps its
   pane with Start again. Nothing installs without the user choosing it.
   Package names were checked against Homebrew, Fedora 43, Debian and Arch on
   2026-11-06: Fedora lacks lazydocker and yazi; Debian lacks lazydocker,
   yazi, k9s and gitui, and has lazygit only from trixie. Tests: the catalog
   unit test (fake shell, `PATH` and managers) and the UI test, which installs
   with a fake `brew` and keeps a fake `sudo` that refuses.
7. [x] **Add applications.** A full-window catalog (`AppCatalog.qml`) with a
   search field, *Installed* (including your definitions) on top and
   *Available* below, curated apps first. Rows show the category and Running
   or Install · Homebrew; a detail strip shows the command or install recipe.
   Return or Open opens an installed app; a missing one is installed only
   through its own **Install with …** button (Return on the row moves focus
   there), so a stray key or click never starts a package manager. Besides the
   ten curated apps, the catalog ships about 200 TUIs generated by
   `scripts/prepare-app-catalog.py`: projects listed in awesome-tuis that
   Homebrew installs, matched by repository URL, with Homebrew's descriptions
   and executables ([sources and licences](../spikes/qt-input/apps-catalog.md)).
   Those install with Homebrew only; apps without a Linux or macOS bottle are
   left out on that platform. Argument-only tools (delta, viu, helm) are
   skipped.
8. **Sidebar trial.** Running apps are shown one of two ways, switched with
   **Sidebar trial** in the catalog's header (or `BAND_APPS_SIDEBAR=workspaces`
   at start): *Apps list*, the Apps panel lists running apps from every
   workspace, like Agents, with Add applications at the bottom; *On
   workspaces*, each workspace row lists its running apps, and the Apps icon
   and Show apps open the catalog. The choice is not saved. One stays once the
   owner has tried both.
9. **Custom setup.** Beyond `[app.ID]`: an `install` recipe for your own apps,
   a `setup` command run once after installing, and later Band-provided
   configuration (themes) for catalog apps, after Omarchy.

Each slice ships on its own and keeps both platforms green.

## Open

- Fedora Atomic hosts: offer a toolbox install, or keep requiring Homebrew.
- Install recipes are not yet run for real on a Mac or on apt or pacman
  systems; they are checked as text.
- The generated catalog is matched automatically. Its entries were read
  through once (2026-11-07), not each tried; a wrong match shows as a wrong
  description or a command that is not found after installing.
- Whether one app can run twice in a workspace. The plan says no: choosing it
  again goes to the running one.
