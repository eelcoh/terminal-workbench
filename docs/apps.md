# Apps (v0.2 plan)

Plan (2026-11-05, revised 2026-11-06). Slices 1–4 are implemented; see
[app terminals](session-service.md#app-terminals) and
[dotfiles](dotfiles.md#apps). Goal from the [roadmap](roadmap.md#v02-apps-and-the-sidebar):
TUIs such as lazygit and btop are first-class and one key away. Running apps
belong to their workspace (owner, 2026-11-05).

Revision (owner, 2026-11-06): no special pane for apps. An app is a standard
pane in the strip; what makes it first-class is how it is found, installed and
set up, after Omarchy's use of TUIs: a catalog, open when installed, install
(Homebrew or the system package manager) when not, and custom setup. The side
panel shipped in slice 4 and an uncommitted dock were dropped.

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
5. **Catalog.** A built-in list of 8–10 well-known TUIs (name, command,
   one-line description, install recipes per platform). `[app.ID]` sections
   add to it or override fields. The Apps panel shows each entry as *Open*
   when its command is found (`command -v` through the login shell, so `PATH`
   matches the app's) or *Install* when not.
6. **Install.** Install runs the recipe in a visible pane, so output and
   prompts (sudo) are the user's to see and answer. macOS uses Homebrew;
   Linux uses Homebrew when present, otherwise the system package manager
   (dnf, apt, pacman) where the package exists (owner, 2026-11-06). Success
   opens the app; failure keeps the pane. Nothing installs silently.
7. **Custom setup.** Beyond `[app.ID]`: per-app configuration or post-install
   steps, after Omarchy.

Each slice ships on its own and keeps both platforms green.

## Open

- The first catalog's entries, and which package names each manager uses.
- Fedora Atomic hosts: the system package manager is `rpm-ostree` or a
  toolbox; whether to offer it or require Homebrew there.
- Whether one app can run twice in a workspace. The plan says no: choosing it
  again goes to the running one.
