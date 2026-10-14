# First integration findings

## Source investigation

Herdr was inspected at local revision
`7bccfd8a73372269b8a2fdc72be3c44efd3dd8d0`.

Its public API has pane reads, text/key input, organization, and agent metadata.
`pane.read` returns formatted terminal state and a revision, rather than an
ordered raw PTY output stream. `pane.resize` takes direction and amount for pane
layout, rather than exact terminal columns/rows. The inspected subscriptions
report organization, matches, and status rather than raw byte delivery.
The client surface transport is a different integration path. Using Herdr as a
graphical backend would therefore require an adapter or protocol additions.

No Herdr implementation has been copied. The first spike uses an independent
PTY runtime to establish process ownership and renderer attachment.

The [current Ghostty embedder header](https://github.com/ghostty-org/ghostty/blob/main/include/ghostty.h)
explicitly describes the full internal API as Mac-oriented and directs external
embedders to the public VT library. The [official Ghostling example](https://github.com/ghostty-org/ghostling)
demonstrates a cross-platform viewer using that public library and Raylib. Our
adaptation pins its revision; it does not depend on a moving upstream branch.

## Implemented

- One Rust-owned PTY per session process, exposed through a private Unix socket.
- A Ghostty VT terminal and Raylib window, attaching without owning the shell.
- Ordered output/resize replay, with terminal query responses suppressed during
  historical replay.
- Bounded queues, runtime scrollback and negotiated snapshot attachment beyond
  the legacy journal cap; replay fallback rejects a truncated journal.
- A launcher that starts/reuses saved sessions, initially Main and Scratch.
- Persistent workspace/terminal creation and renaming with stable session IDs.
- Runtime-owned foreground executable identification, optional subscriptions, and
  agent labels/filtering, with unavailable metadata isolated from terminal transport.
- A searchable terminal overview across workspaces, direct jumps, connection
  labels, and live unseen-output marks.
- A sidebar, two resizable terminal panes, independent focus/selection, and
  unseen-output indicators for unfocused sessions.
- Visible Close, Copy, and Paste controls; VT-backed text/word/line selection,
  highlighted ranges, and plain-text copy with soft-wrap unwrapping.
- Clipboard shortcuts, bracketed paste encoding, and return to live output on input.

## Verification

Linux validation runs inside the Fedora `build-machine` Distrobox. The graphical
test uses its own Xvfb display and Mesa software rendering, leaving the user's
desktop untouched. This establishes X11 behavior; native Wayland and physical
GPU behavior have not been established.

`just check` covers Rust formatting, Clippy, codec tests, and real PTY smoke tests.
`just spike-gui-test` covers the renderer window, shell typing, Ctrl-C, Escape,
resize, a real `vi` edit/save, alternate-screen output, graceful window close,
and reconnection with the same shell PID and retained shell variables. It also
checks real clipboard transfer, forward/reverse/word selection, wrapped Unicode
copy, Unicode and bracketed multiline paste, mouse-app routing with Shift
selection, scrollback copying, and return to live input.

`just workspace-gui-test` verifies four distinct shell PIDs, workspace switching,
focused input and clipboard ownership, per-pane selection, divider resizing of
independent PTYs, hidden-session output, a real `vi` edit/save, reattachment of all
four PIDs and shell variables, and shell exit without closing a sibling pane.

`just management-gui-test` checks GUI create/rename, the same process and shell
variables after renaming, Cancel/Escape isolation, empty-name rejection, desktop
clipboard paste into a name field, third-terminal navigation, saved names and
same-PID reopen. It also checks confirmed stop, foreground-job termination,
surviving shell identities, empty states, and no resurrection of removed sessions.
Overview checks cover name filtering, Escape/no-match input isolation, click and
keyboard jumps across workspaces and to a third terminal, output while open, wheel
scrolling and keyboard navigation at minimum window size. `just check` also validates catalog migration, unique identities,
capacity, invalid-action isolation, and preservation of unsupported metadata.

## Decision

The runtime/viewer boundary is feasible, and the public Ghostty VT library is a
viable parser/state foundation for this experiment. This does not yet establish
Ghostling/Raylib as the final application toolkit. Its frame-based input,
limited glyph loading, and lack of complete desktop text-input behavior
leave substantial work before it can be a daily terminal.

The pinned library now supplies runtime-owned terminal state, detached query
handling, binary snapshot export and negotiated snapshot attachment with ordered
live continuation. New viewers/runtimes reconnect beyond the legacy journal cap;
older running sessions keep replay fallback. See
[runtime hardening evidence and limits](runtime-hardening.md).

macOS build/input/clipboard validation, IME, font fallback/shaping, edge-drag
selection autoscroll, and final UI toolkit selection remain open. Linux clipboard
and wheel scrollback checks pass; native Wayland clipboard behavior is unverified. The workspace sidebar is a
Linux layout prototype with a cross-workspace terminal overview, saved organization
and a management CLI, not a final
workspace service. Live CLI hook compatibility, smoothly scrolling panes,
navigation shortcuts and installable app packaging remain unimplemented.

Confirmed close now removes terminals/workspaces while preserving surviving pane
resources by identity. The runtime's private control socket supports stopping while
attached; the helper records and recovers interrupted close intent before startup.
Empty workspaces/catalogs remain usable. Old running binaries without the endpoint
are rejected before any stop; macOS process-group behavior remains unverified.

Foreground identification now samples the PTY group leader in the runtime and
publishes labels through an optional metadata connection, including while detached.
Linux native-process fixtures and GUI labels/Agents filtering are tested; real
Linux Codex 0.161.0 and Claude Code 2.1.296 recognition now passes. Other CLI
releases and the macOS adapter remain unverified. Executable/launcher
recognition is a heuristic with wrapper/background limitations. Identity alone adds no agent
working/waiting claims. Optional hook observers provide lifecycle status and
a Needs input filter; runtime and GUI fixtures validate transitions, detach,
clearing and legacy subscriptions. Real Linux Codex/Claude hooks now validate completed turns, interactive
questions and same-process detach/reattach. Permission prompts and Mac hooks
remain unverified. See [the live validation record](agent-validation.md). See [hook integration](agent-attention.md).


The overview now defaults to a spatial workspace view with cached live terminal
previews. An isolated Linux test uses fifteen independent shells to verify
never-opened screen updates, unchanged PTY geometry on open/overview resize,
two-axis navigation, filtering, small-window operation, and surviving shell PIDs.
The compact list remains available. The spatial prototype now supports saved terminal moves and reordering. Mac validation is pending.


Spatial Overview now has reversible 180 ms zoom transitions, with pending navigation
committed after closing and a reduced-motion launch option. A queued-input regression
found by fast GUI typing was fixed with bounded lookahead; the first character is
retained even when its press/release occurs within one frame. Pure contract tests
and isolated GUI tests cover both motion modes. Mac validation remains pending.

`just move-gui-test` checks drag-to-move into empty/populated workspaces, reordering,
filtered full-workspace rejection, cancellation, saved placement after reopening
and live shell/socket/geometry continuity. Catalog tests reject invalid moves
before saving, prove moves launch/stop no sessions, and verify live pane reuse.
Native macOS drag validation remains pending.


The first UX/appearance slice introduces shared semantic colors and rounded
controls across workspace, overview and management surfaces. The workspace
client supports original Night/Dawn palettes, imported Ghostty color fields,
live preview, cancellation and saved appearance separately from session state.
Pure parser checks and one/fifteen-terminal Xvfb checks verify import errors, save
failures and session/input/geometry continuity. The loader is a documented color
subset. The offline gallery now adds search, dark/light filters, Dracula and the
four Catppuccin variants from pinned official sources, with their MIT licenses
preserved. Bundled and imported files use the same parser. See [appearance](appearance.md)
and [UX evidence and remaining review gates](ux-spike.md).

The owner has confirmed a native macOS build/run after SSH dependency fetching
was corrected. Full Mac terminal/input/clipboard/gesture acceptance and this
new appearance slice remain unverified. Software-rendered idle CPU is high;
physical GPU and frame-time measurements remain open. No final toolkit decision
has been made.


Appearance preferences now live in a separate versioned client file. Application,
default terminal, workspace and terminal palettes resolve independently; stable
chrome is the default, and optional follow mode follows the active workspace rather
than its focused terminal override. VT defaults, selection colors and color-scheme
query replies use each pane's effective palette. Only appearance/organization events
apply terminal palettes; ordinary frames do not scan preferences or reload files.
Pure state checks cover inheritance, stable-ID moves and transactional parsing.
Native macOS review of these controls remains pending.

### macOS trackpad taps

The owner confirmed startup on the Mac after session recovery, but reported pane
focus requiring a physical click despite system tap-to-click being enabled.
Raylib 5.5's GLFW input only compared final button states; a press and release
within one poll disappeared. The prototype now uses a marked local Raylib
adaptation that latches both edges until the next poll, retaining actual held
state. Selection gestures and terminal mouse encoding process both edges in order.
The build fetches the pinned Raylib rather than accepting an unpatched system
library. The adaptation retains Raylib's zlib license and copyright notices.
The owner subsequently confirmed tap-to-click works on macOS.

The local adaptation is implemented by `scripts/patch-raylib-input.py`, applied
at CMake configure time to `rcore.c` and `platforms/rcore_desktop_glfw.c` from
Raylib tag `5.5`. Keep it until the chosen backend provides per-poll mouse
press/release events that survive a complete click in one poll. Before removing
it, pass the actual callback/query test and the paused-viewer tap regressions.

Validation: the callback/query test fails at the short-tap assertion against
unmodified Raylib 5.5 and passes against the adapted source. The real viewer
test queues both button edges while paused, then confirms focus and subsequent
input in each workspace/pane. A mouse-aware terminal receives press and release
reports for the same-poll tap. `just check`, `workspace-gui-test`, `spike-gui-test`,
`management-gui-test`, `expose-gui-test` (animated and reduced motion), and
`move-gui-test` pass on Linux with isolated Xvfb displays.

### macOS creation and socket paths

The owner confirmed tap-to-click, then found new workspace/terminal creation
failing on the session path check. The reported Mac session directory is 71
bytes. Seeded short IDs fit, but the original 32-character generated terminal
IDs produced 109-byte socket paths, beyond the shared macOS-compatible 103-byte
maximum. New terminals now receive 16-character IDs, checked against all saved
entity IDs before use, giving 93-byte socket paths in that checkout. Existing
IDs, runtime endpoints, theme bindings and catalog schema remain unchanged.
Longer checkout paths still require a shorter location; startup diagnostics now
include the offending socket path and the limit. Native creation retesting is
pending.

The socket-path regression uses real runtimes in a 71-byte absolute session
directory. It verifies workspace and terminal creation, attachment to their new
sockets, unchanged existing socket inodes/PIDs/shell state and saved preferences.
It also covers ID collisions, retained legacy IDs, and rejection before mutation
when the path cannot fit even the shorter filename.

### Working-view navigation

Pane navigation remains client presentation state. No runtime/catalog fields,
identity rules or persistence formats change. A pure focus/window policy clamps
selection and keeps the target within a two-pane viewport. A pure wheel gate
handles fractional input, reversal and bounded momentum. Input is scoped to the
existing navigation header; terminal wheels and keyboard events retain their
normal ownership.

The working-view compositor uses an animated fractional position along a pane
strip. Every intermediate pane crosses the viewport; it does not replace one
visible pair with another. Retargeting evaluates the current position before
choosing a new destination. Intersecting tiles render once per navigation into
reusable textures, using retained terminal state and presentation-only geometry.
Intermediate tiles never request a PTY resize or receive keyboard/pointer input.
The selected destination keeps normal layout/input ownership immediately.

Only intersecting tiles update the render state, so hidden background workspaces
remain outside the presentation path. At half width this is at most three
tiles; thirds can show three full columns and partial columns during motion.
The eight-pane limit bounds texture retention. Steady frames render only
intersecting pane canvases. Target layout can resize a visible pane when its
preferred width changes; mouse guides and animated positions never resize PTYs. Pointer interactions settle motion
before using target geometry. Dialogs, themes, overview entry/exit, catalog reload,
workspace switches and resize also settle it. A live pixel-sampling regression
checks every intermediate pane during an eight-to-one jump, with distinct
backgrounds that cannot be confused with the source or destination.

Terminal glyph rendering now loads bundled JetBrains Mono Unicode mappings and
a licensed Noto Sans Symbols 2 fallback instead of the ASCII-only atlas. Linux
screenshots exercise Claude block art, prompt/status symbols and a monochrome
package icon. The owner confirmed glyphs on Linux and native macOS in Copilot;
specific Retina scale coverage and full shaping remain open.
See [terminal fonts](terminal-fonts.md).

Copilot integration now adds native/npm identity and an invocation-local hook
plugin, with runtime/socket and overview fixture coverage. No user/project hook
settings are edited; native Mac agent acceptance is pending. See
[Copilot testing](copilot.md).

### Ordered terminal input

The live-agent rapid-input finding was reproduced with a raw PTY: legacy input
passed, while Kitty reporting duplicated/reordered text. A marked GLFW callback
bridge now pairs each key with its own text in native order and batches encoded
bytes to avoid per-key packet backpressure. Paused-viewer regressions pass across
legacy and three Kitty configurations at 12 ms and 1 ms. Native Mac typing,
composition, focus and IME acceptance remain open. [Implementation and checks](terminal-input.md).
