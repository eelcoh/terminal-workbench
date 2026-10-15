# Ordered terminal keyboard input

Rapid typing in interactive agents exposed duplicated/reordered characters,
including malformed `/exit` commands. A raw-terminal reproduction isolated the
problem: identical 12 ms synthetic typing passed in legacy mode and failed when
Kitty keyboard reporting was enabled. The old adapter joined all text for a frame
onto its first scanned key, then independently encoded other pressed keys.

The prototype now retains GLFW callback order in a separate terminal event queue.
Each press/repeat is paired with its following character callbacks; releases keep
their own action/modifiers. Character-only commits remain text events. The
operating system's character callbacks provide actual text; the adapter does not
invent text for an unmodified printable press without a character callback.
This follows GLFW's distinction between physical keys and generated text, which
need not map one-to-one. See the [GLFW input guide](https://www.glfw.org/docs/latest/input_guide.html#input_char).

Ghostty remains responsible for legacy/Kitty encoding and negotiated terminal
options. Encoded bytes preserve event order but are batched into transport-sized
packets; sending one packet per key in a paused-window burst had overflowed the
runtime's bounded PTY input queue. No runtime protocol, ownership rules or shared
session state changed. Reconnecting/input readiness and application shortcuts
retain their existing routing. Raylib's original key/text queues remain available
to dialogs and shortcut capture; intercepted-key releases are still blocked.

The terminal callback queue is bounded to 4096 events per poll. Overflow discards
the terminal batch with a log warning rather than sending a truncated command.
Encoding uses a bounded 64 KiB output buffer. Queues are per viewer and are reset
on each poll/focus loss; processing occurs only for the focused, input-ready
terminal. Clipboard paste follows its existing separate bracketed-paste path.

`scripts/patch-raylib-keyboard.py` adds the marked bridge to pinned Raylib 5.5's
GLFW callbacks at configure time. `platform_keyboard.h` isolates that backend;
`terminal_keyboard.h` consumes the events through the existing Ghostty API.
The mouse/display adaptations remain idempotent with the new bridge. Upstream
Raylib zlib notices are preserved. Remove this local bridge only when the selected
toolkit provides equivalent ordered key/text/release input and passes the same
behavioral checks.

## Validation and native acceptance

`just keyboard-gui-test` uses a real Workbench window, runtime and raw PTY reader
in an isolated Xvfb display. Pausing the viewer during injection forces callback
bursts that cross the original UI-queue capacities. Exact text/shifted case,
repeated letters, punctuation and Enter order pass at 12 ms and 1 ms per character
under keyboard flags 0, 1, 3 and 31. The terminal stays connected through subsequent
cases. Pure tests cover callback ordering, repeats, release/modifier retention,
character-only Unicode commits, invalid scalar rejection and the overflow bound.
`just check` and existing shortcut/dialog/clipboard regressions accompany it.

The owner confirmed normal native Mac typing after rebuilding on 2026-10-14.
The automated evidence remains Linux X11/software rendering. For repeat Mac
checks, rebuild/launch with
`just spike`; ordinary shell typing and `wb-copilot` should receive fast text and
repeated letters exactly once. Check `/exit`, shifted punctuation, arrows, held
key repeat, Ctrl-C, clipboard paste and application shortcuts. Separately check
Option/dead-key composition, IME and focus changes with a native keyboard. The
Linux synthetic test does not complete those native input gates, nor does it
choose Raylib/GLFW as the final toolkit.

## Keyboard layouts and composed text

A native Wayland raw-PTY check reproduced German QWERTZ Ctrl-Z sending Ctrl-Y.
Ordinary text already used the OS character callback, but modified keys used
US-position codepoints. Key events now retain a single validated Unicode scalar
from GLFW's layout-specific key name, including releases. Ghostty receives that
unmodified scalar for Kitty reporting and Alt encoding. For suppressed ASCII Ctrl
text, the adapter supplies the layout character to Ghostty's existing control
encoder. Non-Latin control keys keep Ghostty's physical-key fallback; missing or
unsupported key names retain the previous fallback.

This follows [GLFW's layout-specific key-name contract](https://www.glfw.org/docs/latest/group__input.html#ga98293ec4493cfe8e7df8ff22ee402b46).
Character callbacks remain authoritative for ordinary text and dead-key commits.
This does not change application shortcut positions, add a composition UI, or
establish general IME support. No runtime/session protocol changes are involved.
The layout lookup occurs once per native key callback, outside pane/render loops.

`just keyboard-layout-gui-test` uses the private nested Niri seat and standard
XKB keymaps without changing the user's keyboard layout. It validates German
text/Ctrl-Z/Alt-Z, German Ctrl-Z under Kitty flags 1/3/31 including releases,
French AZERTY text/Ctrl-A, and US-international dead-acute + E yielding exactly é.
The same test retains Unicode clipboard, resizing and same-process reattachment
checks. Its artifacts are under `.local/artifacts/keyboard-layouts/`.
The existing X11 rapid legacy/Kitty tests and application routing checks remain
separate regressions.

This is automated Linux input evidence. Physical non-US keyboards, native Mac
Option/dead keys, candidate-window IMEs, layout-switching behavior, accessibility
and broader modifier combinations still need native acceptance.
