# Shortcut settings and navigation

Open Commands, search **Shortcut settings**, and press Enter. Filter the action
list, select an action, and press Enter or click it. Press the desired chord in
its recorder, then **Save** or Enter. **Clear** leaves the action unbound;
**Default** restores its platform default. **Cancel** or Escape preserves the
saved binding. Commands displays the active binding beside each action.

## Default bindings

| Action | Linux | macOS |
| --- | --- | --- |
| Commands | Ctrl+Shift+P | Command+Shift+P |
| Overview toggle | Ctrl+Shift+O | Command+Shift+O |
| New terminal | Ctrl+Shift+T | Command+T |
| New workspace | Ctrl+Shift+N | Command+N |
| Previous / next terminal | Ctrl+Shift+Left / Right | Command+Option+Left / Right |
| Previous / next workspace | Ctrl+Shift+Up / Down | Command+Option+Up / Down |
| Cycle focused terminal width | Ctrl+Shift+R | Command+Shift+R |
| Toggle focused terminal full width | Ctrl+Shift+F | Command+Shift+F |
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
existing dialogs, palette, settings and appearance own their input first.
Overview additionally accepts its toggle binding. Global navigation is not active
inside an overlay. Stop/recovery chords only open the existing confirmation:
Enter cannot terminate or replace a shell. An unavailable bound action is consumed
and reports why; it is not silently sent to a shell.

The recorder supports letters, digits, arrows, navigation/edit keys and F1–F11.
Ordinary keys need Ctrl, Command/Super or Alt/Option. Duplicate chords are rejected
with both action names. Bare Ctrl-C/D/Z are reserved for terminal input;
Escape and F12 are reserved by modal controls and the prototype's screenshot hook.
Text-field editing shortcuts remain desktop conventions and are not configurable
here. This slice does not support multi-key sequences, arbitrary punctuation,
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

The launcher uses separate local files for each platform:
`.local/shortcuts-linux.conf` and `.local/shortcuts-macos.conf`. Set
`WORKBENCH_SHORTCUTS` before `just spike` to use another path. Files use private
atomic saves and apply immediately in the editing window. Other open windows load
changes when opening Shortcut settings; new windows load them at startup. Saving
rereads the latest valid file and edits only the chosen action. Simultaneous saves
are not synchronized by the session service; the final file replacement wins.

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
Editor saves write the complete map. Cross-platform files use Super as the stored
Command modifier; labels display Command and Option on macOS.

## Validation

`just check` covers Linux/Mac defaults, parsing, reserved controls, aliases,
collisions, unbinding, private atomic roundtrips and invalid-file isolation.
`just commands-gui-test` uses live shells to check navigation, configuration,
modal precedence, cancelled/disabled actions, persistence on reopen, raw modified
arrow forwarding, disabled clipboard bindings and ordinary Ctrl-C.
`just management-gui-test window-state-gui-test` covers existing overview/dialog
routing, shell preservation and independent view restoration.
Native macOS navigation/recording/forwarding and native Wayland remain pending.
