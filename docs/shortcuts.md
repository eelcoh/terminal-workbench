# Shortcut settings and navigation

Open Commands, search **Shortcut settings**, and press Enter. Filter the action
list, select an action, and press Enter or click it. Press the desired chord in
its recorder, then **Save** or Enter. **Clear** leaves the action unbound;
**Default** restores its platform default. **Cancel** or Escape preserves the
saved binding. Commands displays the active binding beside each action.

## Default bindings

| Action | Linux | macOS |
| --- | --- | --- |
| Commands | Super+Alt+P | Command+Option+P |
| Overview toggle | Super+Alt+O | Command+Option+O |
| New terminal | Super+Alt+T | Command+Option+T |
| New workspace | Super+Alt+N | Command+Option+N |
| Previous / next terminal | Super+Alt+Left / Right | Command+Option+Left / Right |
| Previous / next workspace | Super+Alt+Up / Down | Command+Option+Up / Down |
| Cycle focused terminal width | Super+Alt+R | Command+Option+R |
| Toggle focused terminal full width | Super+Alt+F | Command+Option+F |
| Find in terminal | Super+Alt+S | Command+Option+S |
| Show shortcuts | Super+Alt+/ | Command+Option+/ |
| Copy selection | Ctrl+Shift+C | Command+C |
| Paste clipboard | Ctrl+Shift+V | Command+V |

Remaining actions start unbound and are configurable, including rename, folder,
appearance, confirmed stops/recovery, settings, and **Send next key to terminal**.
Explicit third/half/two-thirds width actions start unbound. Width cycling runs
half → two thirds → third → half. Full width fills the terminal area and a second
press restores the preferred width. It does not change native window fullscreen.

Navigation stops at the first/last item; it does not wrap. Terminal navigation
uses the existing scrolling transition, and workspace navigation retains each
workspace's focused terminal. Holding a navigation chord permits key repeat.

Modifiers must match exactly. The working view owns application shortcuts;
existing dialogs, palette, settings, terminal search and appearance own their input first.
Overview additionally accepts its toggle and shortcut-help bindings. Help captures
its own input; other application bindings do not run underneath it. Global navigation is not active
inside an overlay. Stop/recovery chords only open the existing confirmation:
Opening Enter cannot terminate or replace a shell; explicitly focus the
confirmation with Tab/Shift+Tab before Enter/Space. An unavailable bound action is consumed
and reports why; it is not silently sent to a shell.

The recorder supports letters, digits, slash, arrows, navigation/edit keys and F1–F11.
Slash is stored/displayed as `/`; `Slash` is accepted as a configuration alias.
Super+Alt+/ refers to the slash key without Shift.
Question mark is not a separate shortcut key.
Ordinary keys need Ctrl, Command/Super or Alt/Option. Duplicate chords are rejected
with both action names. Bare Ctrl-C/D/Z are reserved for terminal input;
Escape and F12 are reserved by modal controls and the prototype's screenshot hook.
Text-field editing shortcuts remain desktop conventions and are not configurable
here. This slice does not support multi-key sequences, punctuation beyond slash,
physical scan-code bindings or IME configuration. Desktop shortcuts captured by
the operating system cannot be reassigned inside this app.

## Send an intercepted key to the terminal

Choose **Send next key to terminal** in Commands, or assign it a shortcut. The
palette closes and the header identifies the armed state. Press the intercepted
chord within ten seconds: application and clipboard shortcuts are bypassed for
that key, and the existing Ghostty encoder sends it using the terminal's modes.
Escape cancels. A mouse click, focus/terminal change, disconnection, or timeout
also cancels. It never takes terminal ownership from another window. Modifier
keys alone do not complete the action. Focus changes are held off for the
forwarded press/release so its input stays with the selected terminal.

Alternatively, Clear a binding to let that chord reach terminal applications
normally. Clearing Copy/Paste also disables their former hardcoded keyboard path;
mouse controls and Commands remain available. Forwarding still relies on Raylib's
current modifier and event model, with the same IME/layout and native-input limits
as ordinary prototype input. Ctrl+Shift+letters without a Raylib text event use
this adapter's existing US-layout letter fallback, allowing Ghostty to emit its
modified-letter CSI-u encoding instead of dropping the key. Other keyboard layouts
and platform-native composition remain unverified.

## Saving and loading

Normal `just spike` launches load shared/platform settings from the managed
dotfile, then `ui/linux/shortcuts.conf` or `ui/macos/shortcuts.conf` under the user
configuration directory. The first launch migrates existing checkout-local
shortcut preferences. UI saves record only differences from the managed baseline;
Default restores the configured binding. See [dotfiles](dotfiles.md) for paths,
precedence, reset and migration behavior.

Set `WORKBENCH_SHORTCUTS` before `just spike` to retain standalone-file behavior.
That explicit file uses full-map saves. Files use private atomic saves and apply
immediately in the editing window. Other open windows load UI changes when
opening Shortcut settings; new windows load them at startup. Managed dotfile
changes require a new window. Saving rereads the latest valid UI file and edits
only the chosen action. Simultaneous saves are not synchronized by the session
service; the final file replacement wins.

A missing file uses defaults. An invalid, conflicting or unsupported file keeps
the current in-memory bindings (defaults on startup) and shows an error in the
palette. Saving refuses to overwrite an invalid file. Fix the file before saving.
The file is client presentation configuration, with no session/service protocol
change. It is not checked from pane rendering or read on every keyboard event.

The format has a version header and one action per line, with no spaces around
`=`. Omitted actions retain platform defaults. When an older file assigns a new width
shortcut to an existing action, that new width action starts unbound rather than
invalidating the existing customization. Use `none` to unbind an action:

```text
workbench-shortcuts-v1
terminal.next=Ctrl+Shift+J
terminal.previous=Ctrl+Shift+K
forward=F8
```

Command/Cmd/Super, Alt/Option, and Ctrl/Control are accepted modifier aliases.
Standalone editor saves write the complete map; managed UI saves write differences. Cross-platform files use Super as the stored
Command modifier; labels display Command and Option on macOS.

## Shortcut-help overlay

Open help with **Command+Option+/** on Mac or **Super+Alt+/** on Linux, or choose
**Show shortcuts** in Commands. It groups navigation, workspace/terminal actions,
widths, appearance, clipboard and help, showing current bindings and **Unbound**
for actions without a chord. The normal window uses two columns; narrow windows
use one scrollable column. Arrows, Page Up/Down, Home/End or vertical wheel/trackpad
scroll the list while its body is focused. Tab/Shift+Tab visits the body, Edit
shortcuts and Done; Enter/Space activates a focused button. Escape, Done or the help binding closes it. Enter or **Edit
shortcuts** opens Shortcut settings. Read-only help rows never dispatch actions.

Help can sit over the working view or Overview, preserving the underlying focus,
query and selection when closed. Existing dialogs, appearance, Commands and the
recorder retain their own input precedence. Terminal shortcuts apply in the working
view; showing them in help does not activate them in Overview. Help consumes typed
text and chords while runtime output continues parsing. Opening help and resizing
its overlay preserve PTY geometry; closing returns to ordinary working-view sizing.
Help is temporary presentation state and is not restored on window reopen.

Bindings reload when help opens and labels are cached for that opening, with no
preferences I/O in drawing. The static list body uses one reusable render texture,
refreshed for reopening, scrolling and size changes; hover controls remain live. Reopening reflects saved customizations. All bound Mac
application defaults now use Command+Option, except Command+C/V for copy/paste.
Existing saved bindings remain intact, including earlier Command+Shift+P/O and
Command+T/N; use **Default** in each action's recorder to adopt the new defaults.
If a changed or new default conflicts with an explicitly saved customization, the
unspecified action is left unbound; the custom binding wins.

Native Mac slash/Option input, non-US keyboard layouts and physical trackpad/visual
acceptance remain pending. See [the roadmap](roadmap.md#current-ux-slice-shortcut-help-overlay).

## Validation

`just check` covers Linux/Mac defaults, parsing, reserved controls, aliases,
collisions, unbinding, private atomic roundtrips, slash aliases, Mac modifier
policy, complete help groups, bounded scrolling and invalid-file isolation.
`just commands-gui-test` uses live shells to check navigation, configuration,
modal precedence, cancelled/disabled actions, persistence on reopen, raw modified
arrow forwarding, disabled clipboard bindings and ordinary Ctrl-C.
`just management-gui-test window-state-gui-test` covers existing overview/dialog
routing, shell preservation and independent view restoration.
Native macOS navigation/recording/forwarding and native Wayland remain pending.

`just shortcuts-gui-test` exercises live help entry/toggle, custom/unbound maps,
slash recording, Overview layering, settings handoff, modal isolation, light/dark
appearance, narrow-window scrolling and retained shell state/geometry, with ordinary
and reduced motion. `just shortcuts-scale-gui-test` profiles fixed geometry with
one and fifteen populated terminals.

Terminal text actions are `terminal.font.settings`, `terminal.font.increase`,
`terminal.font.decrease`, and `terminal.font.reset`. Size shortcuts default to
Super+Alt+= / - / 0 on Linux, Command+Option+= / - / 0 on Mac. Equal/Minus
are accepted key names for configuration and recording. The settings action
is unbound by default and accessible in Commands and the hamburger menu.
See [terminal font controls](terminal-fonts.md#choosing-a-font-and-size).

See [terminal search](terminal-search.md) for match navigation, input ownership and viewport restoration.

## Matching application modifiers

Linux application/navigation defaults use Super+Alt, matching Command+Option on
Mac. Copy/paste retain Ctrl+Shift+C/V on Linux and Command+C/V on Mac. All labels
in Commands, menus, help and the recorder use the effective configured bindings.

Existing saved chords and explicit `none` values remain intact, including earlier
Ctrl+Shift defaults. Omitted actions inherit the new defaults; full-map files keep
their complete saved map. Use **Default** in an action's recorder to adopt its
current platform/managed binding. No preference files are rewritten on startup.

Compositors get shortcuts first. The Niri/Zirconium configuration on the developer
machine assigns Super+Alt+S to toggle Orca, which conflicts with Find in terminal.
Use Commands for search, remap Workbench search in Shortcut settings, or adjust
that compositor binding yourself if desired. Workbench does not alter desktop
shortcuts. Other local Super+Alt Workbench defaults have no exact match in the
inspected Niri/DankMaterialShell bindings. The owner confirmed the shortcut change
on 2026-10-14. Non-US keyboard layouts and the complete compositor-conflict matrix
remain unverified; Xvfb checks do not validate compositor delivery.

Linux validation passes `just check`, `just commands-gui-test`,
`just shortcuts-gui-test` (normal/reduced motion), `just navigation-gui-test`
(one/fifteen terminals and reduced motion), `just search-gui-test`,
`just font-settings-gui-test` (one/fifteen), and `just text-edit-gui-test`.
These cover new defaults, custom/save/reset/reopen behavior, modal input ownership,
unchanged clipboard/control keys, and legacy/Kitty forwarded input. Pure tests
also preserve complete old Linux maps, sparse overrides and explicit unbindings.
The test client now retries connection refusal briefly because socket-path
publication precedes listener readiness; protocol/ownership rejections still fail.


## Qt candidate: managed application shortcuts

`just toolkit-ui` now reads supported application chords from `config.conf` at
startup, using the same `[shortcuts]`, `[linux.shortcuts]` and `[macos.shortcuts]`
precedence and versioned field validation as the daily viewer. The managed file
stays read-only. Commands, menus, the New terminal tooltip and shortcut help show
active bindings rather than fixed defaults. Restart the study after file edits.

For example, these bindings open Commands, help and workspace rename:

```text
workbench-config-v1

[shortcuts]
commands = F10
shortcuts.help = F9
workspace.rename = F8
```

The Qt adapter currently supports Commands, Overview, Appearance, workspace
creation/name/folder/navigation, terminal creation/confirmed close/navigation,
width cycle/full, font settings/sizing and shortcut help. Use the corresponding
action IDs:

- `commands`, `overview`, `appearance`, `shortcuts.help`
- `workspace.new`, `workspace.rename`, `workspace.folder`,
  `workspace.previous`, `workspace.next`
- `terminal.new`, `terminal.stop`, `terminal.previous`, `terminal.next`
- `terminal.width.cycle`, `terminal.width.full`
- `terminal.font.settings`, `terminal.font.increase`,
  `terminal.font.decrease`, `terminal.font.reset`

`none` unbinds an action, allowing that former chord to reach the terminal. Actions unavailable in
the current view remain consumed. Boundary navigation still reveals the selected
pane after manual scrolling. Qt-only move defaults yield when a configured action
uses their chord.

Command/Super maps to Command on Mac and Super on Linux; Ctrl means the physical
Control key on both. Mac labels distinguish Command and Control. Missing config
uses defaults; malformed or conflicting configuration falls back transactionally
with a startup notice. Unused appearance files are never opened by this adapter.
Native text-field editing and terminal copy/paste keep their platform shortcuts.
An application binding conflicting with those native clipboard chords is rejected.

This slice imports managed application shortcuts only. Existing `ui/` override
files, the shortcut recorder/editor, clipboard remapping, one-shot forwarding and
unsupported actions have not moved to Qt. Unsupported actions are not intercepted.
The working viewer retains those workflows. Qt settings stay independent of the
session service and do not change runtime ownership or protocols.

`just toolkit-shortcuts-test` checks shared/platform precedence, exact Mac modifier
mapping, unbinding, unsupported actions, reserved controls, clipboard conflicts and
move-default collisions without a display. `just toolkit-ui-test` exercises real
F10/F9/F8 remaps, former-chord passthrough, dynamic hints/help, modal isolation and
existing workflows on private X11 and Wayland desktops. X11 uses the established window-directed key path, with physical XTest function
keys to avoid Xdotool’s incorrect Alt synthesis. Native Mac shortcut review remains pending.
