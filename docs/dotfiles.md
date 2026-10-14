# Dotfiles

Workbench can load shared appearance, terminal-font and shortcut settings outside the source
checkout. It reads your managed file and writes UI edits to separate files.
Shell dotfile installation, arbitrary installed fonts, workspace defaults and live reload
are follow-up work.

## Getting started

Run `just config-path` to print the managed file location, or `just config-init`
to create a commented example. Init preserves an existing file or symlink.

| Platform | Default directory |
| --- | --- |
| Linux | `$XDG_CONFIG_HOME/terminal-workbench`, otherwise `~/.config/terminal-workbench` |
| macOS | `~/Library/Application Support/Terminal Workbench` |

Set `WORKBENCH_CONFIG_DIR` to use another directory, for example a shared
dotfiles checkout. Directory and `config.conf` symlinks are supported. Theme
paths are relative to the configured directory, even if the config file itself
is a symlink. Share `config.conf` and your `themes/`; leave generated `ui/` files
out of your dotfiles repository.

```text
workbench-config-v1

[appearance]
application-theme = Workbench Night
terminal-theme = themes/my-theme
follow-workspace = false

[font]
family = Fira Code
size = 18

[macos.font]
size = 16

[shortcuts]
forward = F8

[linux.shortcuts]
terminal.new = Ctrl+Alt+T

[macos.shortcuts]
terminal.new = Command+Option+T
```

This is a small versioned section/key format, not TOML or full Ghostty config.
Values are unquoted, including names/paths with spaces. Blank lines and comments
starting with `#` are allowed; inline comments, interpolation and includes are
not supported. End each line with a newline. Duplicate keys in a section,
unknown fields, invalid chords and final shortcut conflicts reject the whole
managed file. Missing files use defaults. An invalid file stays untouched: the
window opens with built-in defaults and valid UI preferences, with a diagnostic
in the viewer log and a startup notice.

## Appearance and shortcuts

Appearance supports `application-theme`, `terminal-theme` and
`follow-workspace = true/false`. Themes may name any bundled gallery palette,
including `Workbench Night`, `Workbench Dawn`, `Dracula` and the Catppuccin
palettes, or point to a [supported Ghostty color file](appearance.md).
File paths may be absolute or relative to the configured directory; `~` and
environment variables inside values are not expanded.

Both appearance and shortcuts allow `[linux.appearance]`, `[macos.appearance]`,
`[linux.shortcuts]` and `[macos.shortcuts]`. The platform section always wins
over shared settings, regardless of their order in the file. Omitted shortcuts
retain platform defaults. Use action IDs from [shortcut settings](shortcuts.md),
and `none` to clear a binding. Platform sections keep Mac Command+Option and
Linux Super+Alt defaults while sharing other preferences, or retain older/custom
Ctrl+Shift bindings explicitly.

## Terminal fonts

`[font]` accepts `family = JetBrains Mono` or `family = Fira Code` and integer
`size = 10` through `32` in logical points. Defaults are JetBrains Mono, 16 pt.
Both faces and symbol fallback are bundled on Linux/Mac. `[linux.font]` and
`[macos.font]` override either field independently, regardless of section order.
Unknown families and invalid sizes reject the configuration transactionally.

Terminal text settings preview font/size for the current window; Apply saves
differences from managed defaults in its local window view file. Cancel restores
the prior font and size. Use defaults resets both to configured values; Reset
terminal text size resets only size. A new window inherits managed defaults, and
an existing window restores its own overrides. Unchanged fields inherit later
dotfile edits on reopen. This never rewrites your config or restarts shells.

## Precedence and UI edits

Global appearance and shortcuts resolve as:

1. Built-in platform defaults.
2. Shared managed settings.
3. Matching platform managed settings.
4. UI overrides in `ui/linux/` or `ui/macos/` below the configuration directory.

Shortcut editing saves only differences from the managed baseline; Default
returns that action to its configured binding. The workspace-first picker saves
workspace/terminal palettes with local session identities. Automatic chrome-follow
saves only its changed global field, preserving other managed defaults. Global
palette choices remain dotfile settings; existing global UI overrides still load.
An explicit `follow-workspace = false` keeps chrome fixed during workspace theme
selection. Cancel never saves.
UI files use private, atomic replacements. The managed file, comments and
symlink remain untouched. Other windows reread shortcut UI edits when opening
Shortcut settings; appearance UI edits load when a new window opens. Managed
settings/theme-file changes require a new window in this slice.

Delete the generated shortcut or appearance override file to return to managed
settings on the next window launch. Migrated appearance preferences initially
override all global fields to preserve the previous look. Remove both
`appearance.state` and an older migrated `appearance.conf` if you want to reset
all global appearance overrides. Keep the `.migrated-v1` marker so removed
preferences are not copied back. Files with invalid UI data are kept and reported;
saving refuses to overwrite them until repaired or explicitly removed.

Workspace/terminal theme overrides stay local in `.local/sessions/appearance.state`
and continue to override default terminal colors. They contain machine/session
identities and are not dotfiles. Window restore state, catalog, sockets and agent
credentials also remain local. Follow-workspace chrome uses the active workspace
palette. Explicit `WORKBENCH_THEME` remains a terminal-default import override.
Explicit `WORKBENCH_SHORTCUTS` retains the existing standalone shortcut-file
behavior, including full-map saves, instead of the managed/UI shortcut layering.

## Migration and scope

The first `just spike` launch on each platform copies existing `.local/shortcuts-<platform>.conf`
and global colors/follow mode from `.local/sessions/appearance.state`, or the
older `appearance.conf`, into the per-user UI directory. Old source files stay
intact. Workspace/terminal identities are excluded from the global copy.
Existing destination files win. Complete records are published exclusively,
so concurrent launchers cannot truncate or overwrite preferences. A per-platform
marker prevents subsequent launches from restoring overrides you removed.
Older partial shortcut files retain their prior default-collision migration
behavior; their next UI save uses sparse overrides.

The launcher still runs a checkout-local build and organization service. Moving
preferences outside the repository does not package the app or relocate live
sessions. Global and scoped appearance records are prepared before replacements,
and ordinary save failures retain the current preview/cancel workflow. They are
independently atomic files, not a transaction across a machine crash.

## Validation

`just check` covers Mac/Linux path discovery, custom directory/symlink handling,
once-only migration and local identity separation, platform precedence, custom
themes, malformed/conflicting settings, sparse shortcut inheritance and global
appearance field masks. `just dotfiles-gui-test` exercises the real viewer with a
live shell: managed themes, UI saves, new managed theme values on reopen,
cancel/failed-save handling and malformed settings, retaining PID/socket/grid.
The Linux viewer build, full `just check`, dotfiles GUI check, one/fifteen-terminal
theme regressions and ordinary/reduced-motion shortcut-help checks pass.
Native Mac filesystem and keyboard review remains pending.
