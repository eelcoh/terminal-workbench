# Dotfiles

Workbench can load shared appearance, terminal-font and shortcut settings outside the source
checkout. It reads your managed file and writes UI edits to separate files.
Shell dotfile installation and arbitrary installed fonts are follow-up work.
Qt live configuration and creation defaults are described below.

## Getting started

Run `just config-path` to print the managed file location, or `just config-init`
to create a commented example. Init preserves an existing file or symlink.

| Platform | Default directory |
| --- | --- |
| Linux | `$XDG_CONFIG_HOME/band`, otherwise `~/.config/band` |
| macOS | `~/.config/band` |

Settings from before the rename to Band (`~/.config/terminal-workbench`) are
copied once the first time the new directory is missing; the old folder stays.
Files with the old `workbench-config-v1` and `workbench-shortcuts-v1` headers
are still read; new files are written as `band-config-v1` / `band-shortcuts-v1`.

Set `BAND_CONFIG_DIR` to use another directory, for example a shared
dotfiles checkout. Directory and `config.conf` symlinks are supported. Theme
paths are relative to the configured directory, even if the config file itself
is a symlink. Share `config.conf` and your `themes/`; leave generated `ui/` files
out of your dotfiles repository.

```text
band-config-v1

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
palette. Explicit `BAND_THEME` remains a terminal-default import override.
Explicit `BAND_SHORTCUTS` retains the existing standalone shortcut-file
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


## Qt candidate scope

`just toolkit-ui` reads managed shared/platform font defaults and supported
application shortcuts. The adapters validate existing field syntax and shortcut
rules, with defaults and a visible notice when invalid. Font and shortcut adapters
ignore unused appearance assets. The Qt workspace theme adapter reads only
`terminal-theme`; legacy `application-theme` and `follow-workspace` fields stay
syntax-checked but do not introduce additional color layers. Qt chrome follows
the workspace palette.

Terminal text settings preview changes across the window; Apply saves sparse
platform UI font defaults in `ui/<platform>/terminal-font.json`. Cancel leaves
the saved defaults untouched. Qt application shortcuts load the existing
platform `ui/` layer and their editor saves sparse overrides there, preserving
managed files. Per-session font restoration remains a follow-up. See
[Qt fonts](terminal-fonts.md#qt-candidate-terminal-text-and-dotfile-defaults) and
[Qt shortcut scope](shortcuts.md#qt-candidate-managed-application-shortcuts).


## Qt theme directory

Place Ghostty palettes in the configuration root’s `themes/` directory and reopen
Appearance to discover them. TerminalColors `.default` downloads work unchanged;
no import wizard or metadata entry is needed. **Appearance → ? → Open themes folder** opens the native
location. Source files and symlinks remain untouched. See [drop-in themes](theme-directory.md).

On macOS, startup copies existing preferences from the previous
`~/Library/Application Support/Terminal Workbench` location once. Managed config,
themes and Mac UI settings are copied only where a destination does not already
exist; existing destination symlinks are left alone. Original files stay intact,
and session/runtime data is never copied. Explicit `BAND_CONFIG_DIR` overrides
skip this migration. A marker prevents removed settings/themes from reappearing.


## Qt live font and shortcut configuration

The Qt candidate now follows changes to `config.conf`,
`ui/<platform>/terminal-font.json` and `ui/<platform>/shortcuts.conf` without
restarting terminals. The application menu and Commands expose **Configuration**;
the panel shows the managed file and UI override destinations, opens their folder
and provides Reload. Commands also offers **Reload configuration**.

Shared/platform rules and sparse UI precedence stay the same. A reload prepares
complete font, shortcut and workspace-theme candidates before publishing any. Malformed
managed data, invalid UI overrides or conflicting shortcuts keep the last valid
maps and show a reload error. Repairing the files retries automatically; manual
Reload is available too. Deleting a UI override restores managed inheritance.
An unreadable or malformed selected `terminal-theme` also rejects the complete
live update. Legacy application-theme files are unused and are never opened.

File and parent-directory notifications are debounced outside terminal paint,
parse and presentation loops. Atomic file replacement and managed-file symlink
retargeting are followed. While Appearance, Terminal text or Shortcut settings is open,
external updates wait so the editor keeps its draft. Existing stale-save checks
still reject Apply after an external change; close the editor to accept the new
valid files. Reload never writes the managed file, comments, symlink or UI layers.

Saved font/shortcut edits now propagate to other open Qt windows. A local,
temporary font-size shortcut stays local when persisted font values did not
change; changing the saved font choice updates all those windows. Existing
workspace themes, session organization, terminal identities and ownership are
independent of reload. Per-session font restoration remains a separate follow-up. This behavior applies
to the Qt candidate; the earlier sections retain the `just spike` behavior.

Validation: `just toolkit-configuration-test` covers transactions, peer updates,
last-valid retention, deferred drafts, temporary zoom, override deletion and
symlink replacement. `just toolkit-configuration-ui-test` runs real-reader checks
on X11 and Wayland for panel/menu/palette access, font/chord updates, invalid
reload and draft retention without PTY input or reader replacement. Native Mac
filesystem/interaction review remains pending.

For a Mac review, install/activate/open the new preview, then open Configuration.
Run `just toolkit-preview open ID` twice (replace `ID` with the installation
ID) to open two windows. The updated Mac launcher uses `open -n`. Give each
window its own terminal. Change a managed font value or a saved font UI setting;
both windows should follow. Existing UI overrides win over managed values, so
use **Use dotfile defaults → Apply** when testing managed font inheritance. A
malformed file should produce a notice while retaining the previous working
font and chords; repairing it should recover. Leave Terminal text open while
editing the file to check that its draft survives until you close the dialog.


## Qt workspace appearance defaults

`[appearance] terminal-theme` sets the palette inherited by workspaces without
an explicit choice. `[macos.appearance]` and `[linux.appearance]` override it.
A bundled name or a Ghostty file path works; relative paths resolve beside
`config.conf`, including TerminalColors `.default` downloads:

```text
band-config-v1
[appearance]
terminal-theme = themes/miasma-default
[macos.appearance]
terminal-theme = Workbench Dawn
```

Only the active platform's selected file is read. File replacement, deletion,
repair and symlink retargeting participate in the same automatic reload as
fonts and shortcuts. A missing or invalid selected file keeps the last working
configuration and shows a diagnostic. Startup uses built-in workspace colors
when the selected file cannot load. Reload never restarts shells or executes
non-color Ghostty settings.

Appearance shows **Use default** and the configured palette name. This previews
inheritance for the selected workspace; **Apply** saves it and **Cancel** keeps
the previous choice. Selecting a palette and applying it saves an explicit,
immutable workspace snapshot. Explicit choices keep their colors when managed
files change or disappear. Inheriting workspaces follow future default edits,
including after a window restart. Existing saved workspace snapshots remain
explicit to preserve their previous appearance; use **Use default → Apply** to
opt them in. New workspaces inherit automatically.

Validation: `just toolkit-configuration-test` covers builtin/platform selection,
custom-file notifications, invalid-file retention and transaction rollback,
preview deferral and immutable slot reuse. `just toolkit-view-state-test` covers
backward-compatible, typed inheritance persistence. `just toolkit-ui-test`
checks real X11/Wayland terminals, Apply/Cancel, inherited versus explicit
palettes, live edits and restore. Native Mac acceptance remains manual.


## Qt workspace and terminal creation defaults

The Qt candidate accepts shared `[workspace]`, `[linux.workspace]` and
`[macos.workspace]` sections. Platform fields override shared fields independently,
regardless of section order:

```text
[workspace]
directory = ~/Code
terminal-width = 1/3

[macos.workspace]
terminal-width = 2/3
```

Add these sections to your existing `band-config-v1` file. Omit `directory`
to prefill New workspace with the selected workspace's saved folder (or the
launch default when there is no workspace). A configured folder replaces that
prefill; you can edit it or Browse before Create. Absolute paths, `~`, `~/path`
and paths relative to the directory containing `config.conf` are supported.
Spaces and Unicode stay literal. Other-user `~name` expansion and environment
interpolation are unsupported. Theme-file values retain their existing path
rules; home expansion here applies to the workspace folder field.

Folder availability is checked by the session service on explicit Create.
A disconnected drive or missing default folder does not block unrelated settings
from reloading. Create reports the ordinary folder error, retains the form, and
lets you choose a usable folder. Configuration does not create project folders.
Existing workspace folders and running shell directories remain unchanged.
New terminals in an existing workspace continue using that workspace's saved
folder; the global folder preference applies to New workspace.

`terminal-width` accepts `1/3`, `1/2`, `2/3` or full width `1/1` (also `3/3`),
defaulting to `1/2`.
It sets the initial pane width for newly added terminals, including the first
terminal of a new workspace. Saved window widths restore as before; later
configuration changes do not resize existing panes. A window without saved
layout starts with the current configured width. Width remains client
presentation state, separate from the session catalog.

Both fields reload with fonts, shortcuts and the workspace theme. Invalid widths,
duplicates and unknown fields reject the complete live update, preserving the
last valid settings. New workspace, Rename and Workspace folder dialogs defer
updates until they close so their drafts survive. Configuration shows the
resolved default folder and width. Defaults are read-only managed preferences;
Workbench never writes them back into your dotfiles or introduces a second UI
override layer for these fields.

Validation: `just toolkit-configuration-test` covers platform fields, relative,
home and Unicode folders, invalid-update retention, deferred edits and deletion.
`just check` exercises the shared C parser and existing runtime folder contracts.
`just toolkit-ui-test` exercises real X11/Wayland input, folder prefills, retained
drafts, missing-folder errors, independent new pane widths and preserved readers.

For the installed Mac app, build with `git pull` then `just toolkit-install`.
Close preview windows, run `just toolkit-preview activate ID` with the new ID,
then `just toolkit-preview open ID`. Set `directory` to an existing project
folder and `terminal-width = 1/3`; New workspace should prefill the folder and
new terminals should start at one third. Change the default to `2/3` and add
another terminal: the earlier pane should retain its size. Leave New workspace
open while editing the managed file: its draft should stay intact until closing.
`just toolkit-workbench` instead opens checkout-local sessions by default; it
does not update or activate the installed app.


## Qt workspace palettes across windows

Applying Appearance now saves a session-scoped workspace choice. Other windows
showing that workspace follow the applied palette, including **Use default →
Apply**. Managed default-theme edits remain live for inheriting workspaces.
Preview and Cancel stay local to the editing window; they never publish colors.
This also works when another window owns all the workspace's terminals. Changing
appearance never reconnects, recovers or transfers a terminal.

Each workspace has a private, bounded JSON preference in the session's
`qt/workspace-appearance/<workspace-id>.json` directory. Configuration shows that
folder. The files contain either a default marker or an immutable color snapshot,
independent of source-theme availability. These contain local session IDs and
belong with session data, outside your portable dotfiles. Different sessions,
including the checkout and installed app's default namespaces, remain separate.

Existing window profiles keep their saved colors until a shared choice is
applied. A shared choice then takes precedence over every window's older snapshot.
Current snapshots continue to be cached in each view for compatibility and
fallback. Removing a shared preference leaves those cached colors available;
use **Use default → Apply** to deliberately reset all windows for a workspace.
Inactive workspace files are retained with the session's other metadata and
cannot affect unrelated IDs.

Files use atomic owner-only writes and independent per-workspace locks. An open
Appearance dialog retains its draft and original comparison state. If another
window applies a different choice to that workspace, Apply reports the conflict
and keeps the editor open. Cancel and reopen to edit the newer choice. Changes to
different workspaces save independently. Invalid or oversized files retain the
last working palette in existing windows and show a notice; Apply refuses to
overwrite invalid data. Repairing a file is followed automatically. File watches
and snapshot parsing run outside terminal paint, input and parse loops.

Validation: `just toolkit-workspace-appearance-test` covers independent saves,
stale drafts, locks, restarts, private permissions, invalid/oversized retention
and repair, bounded identities and write failures. The real X11/Wayland UI suite
checks local preview/Cancel, an actual peer-window Apply, stale draft rejection,
shared default inheritance and unchanged terminal owners, catalog, readers and
grids. Native Mac interaction acceptance remains manual.

After installing/activating the new Mac preview, run `just toolkit-preview open
ID` twice. Apply a workspace theme in either window and check the other window.
Leave Appearance open in window A; apply another theme to the same workspace in
window B. A should keep its preview, reject Apply, and adopt B's saved theme when
you Cancel. **Use default → Apply** should return both windows to the configured
workspace default.

## Qt window view defaults

The Qt candidate accepts `[view]`, `[linux.view]` and `[macos.view]`:

```text
[view]
navigation-pinned = true
reduced-motion = false

[macos.view]
reduced-motion = true
```

Both fields accept only `true` or `false`; built-in defaults are false. Platform
fields override shared fields independently, regardless of section order. They
join the existing live configuration transaction: malformed values keep all
previous valid settings, and open settings editors defer updates.

New windows follow these defaults. Pinning/unpinning navigation or toggling
Reduced motion makes that setting a saved choice for the current window.
Other windows keep their own choices. **Configuration → This window** shows
whether each setting follows dotfiles or a saved window choice. **Use view
defaults** clears both choices immediately; it preserves size, layout, filters,
workspace themes and terminal sessions. Subsequent dotfile edits then take effect
without restarting the app. The default markers survive window close/reopen;
cached boolean values do not freeze inheritance.

Older window files without default markers preserve their existing explicit
motion/pin choices; a missing optional pin field inherits. Use view defaults to
opt into managed values. At narrow widths, pinned navigation still collapses to
a drawer without discarding the preference. This is Qt client presentation state,
separate from workspace palettes, catalog organization and runtime ownership.

To validate on Mac, install, activate and open the new preview. Add the example
to `~/.config/band/config.conf`, open Configuration and choose
**Use view defaults**. Change either managed value and watch it update. Toggle
that setting using the navigation pin or Reduced motion menu action; later file
changes should preserve the window choice. Close/reopen to check restoration,
then reset again. Open a second window to check independent choices. Live shells
should retain their processes throughout. Linux model and real X11/Wayland tests
cover transactions, inheritance, reset, legacy views and restart; native Mac
interaction review remains required.

## Waiting-agent bindings

The shared and platform shortcut sections accept `agent.input.next` and
`agent.input.previous`, defaulting to Super+Alt+J/K (Command+Option+J/K on Mac).
The existing sparse UI layer and collision rules apply: explicit older bindings
win over new inherited defaults. No configuration migration or rewrite is needed.
See [waiting-agent navigation](shortcuts.md#waiting-agent-navigation-in-qt).
