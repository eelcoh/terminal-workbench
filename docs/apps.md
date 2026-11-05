# Apps (v0.2 plan)

Plan (2026-11-05). Slice 1 (runtime and catalog) and slice 2 (definitions)
are implemented; see [app terminals](session-service.md#app-terminals) and
[dotfiles](dotfiles.md#apps). Nothing in the UI starts an app yet. Goal from the [roadmap](roadmap.md#v02-apps-and-the-sidebar):
TUIs such as lazygit and btop live next to the strip and are one key away.
Running apps belong to their workspace (owner, 2026-11-05).

## Model

An app is a runtime terminal like any other, so it survives detach, window
close and service handover. The difference is in the catalog and the client:

- **Catalog.** A terminal entry may carry `"app": {"id", "command",
  "directory"}`, saved when the app starts. It stays in its
  workspace's `terminals` list, so moves, recovery and shutdown need no new
  paths, but the strip skips it.
- **Runtime.** The service starts an app's runtime with the command from its
  definition instead of a plain shell. The runtime runs it as
  `$SHELL -l -c '<command>'`, so `PATH` from the login shell (Homebrew,
  `~/.local/bin`) applies and quoting is the shell's. It starts in the
  definition's directory, or the workspace folder by default.
- **Exit.** When the program exits, the pane closes. The service removes an
  ended app terminal on the next catalog change or window open, and the client
  (slice 4) closes its pane at once.
- **After a reboot**, stopped apps are dismissed, not restarted. Starting one
  again is one key. **Start all stopped shells** skips them.
- **Client state.** Which app is in the side panel, and the panel's width, are
  per-window view state, like scroll positions today.

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
3. **The rail, one panel per icon.** Replace the drawer's Workspaces/Agents
   switch. Clicking an icon opens its panel and clicking the active icon closes
   it; pinning stays. This lands before the Apps panel, so the panel has a
   place to go.
4. **Apps panel and side panel.** The panel lists definitions; choosing one
   starts it in the current workspace, or shows it if it is already running. A
   running app opens in a side panel beside the strip, with its own width. It
   is a live terminal with normal input, selection and search.
5. **Dock.** Small rail icons under the fixed ones for the current workspace's
   running apps. Click shows or hides the app; it keeps running when hidden.
   Command-P / Super-P lists apps and does the same. Shortcuts are designed
   afresh, per AGENTS.md.

Each slice ships on its own and keeps both platforms green. Slices 1 and 2 do
not change the UI.

## Open

- An app icon in the dock: a letter badge from the name, or an optional
  `icon` key later.
- An app that fails to start (owner's first trial, 2026-11-05: `htop` missing
  inside the Distrobox, exit 127) vanishes without a word. Slice 4 keeps the
  pane of an app that exits non-zero within a few seconds, with its exit code
  from the `.lifecycle` file and its last output, until it is dismissed.
- Whether one app can run twice in a workspace. The plan says no: choosing it
  again shows the running one.
