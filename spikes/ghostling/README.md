# Ghostty terminal integration spike

This viewer adapts the official Ghostling example to attach to a separate Rust
session runtime. It is an experiment, not the final desktop UI or renderer.

Pinned dependencies:

- Ghostling: `63842bf8e5e481160f81d348da9ff6fd27986798` (MIT).
- Ghostty VT: `f64f4aca2c29b554d111b36c3d946a9bddd159ff` (pinned by Ghostling).
- Raylib: 5.5 (pinned by Ghostling).
- Embedded JetBrains Mono font: SIL Open Font License.

`scripts/prepare-ghostling.py` reads that exact upstream tree and generates the
adapted source beneath `.local/`. It preserves the upstream license and font
notice and leaves the pinned dependency source files unchanged. GitHub fetches
use SSH for Ghostling, Ghostty VT and Raylib; preparation also migrates an
existing Ghostling checkout origin to SSH. `session_bridge.h` is our
transport adapter. No Herdr code or keybindings are copied.

Build with `just spike-build`. On Linux, the build needs CMake, Ninja, a C/C++
compiler, Zig 0.16.0, and Raylib's X11/OpenGL development dependencies. On macOS,
use Xcode Command Line Tools, CMake, Ninja, and Zig 0.16.0. macOS is unverified.

Start the foreground session server in one terminal:

```sh
mkdir -p .local/sessions
cargo run --bin workbench-session -- "$PWD/.local/sessions/spike.sock" /bin/sh
```

Attach a window from another terminal:

```sh
just spike-run "$PWD/.local/sessions/spike.sock"
```

Click **Close** in the top-right header to detach the window. The shell keeps
running, even on desktops without native window decorations. Running the attach command again connects to
the same shell and reconstructs terminal contents. Type `exit` in the shell
to end the session. The server runs in the foreground for this spike; there is
no login service or installer yet.

On this workstation, run both commands inside `distrobox enter build-machine`.
The socket and build directories live in the shared project directory.

## Deliberate limits

- One attached viewer per session. Each terminal runs in a separate runtime
  process; initially there are four sessions across Main and Scratch.
  A second viewer of the same session shows "Open in another window". To test
  reconnection, close the first window and launch `just spike` from another
  terminal application; running it inside Workbench opens a competing viewer.
- Runtime-owned state has bounded scrollback. New runtimes/viewers reconnect
  through snapshots even after 16 MiB of output. Legacy replay fallback keeps
  its 16 MiB attachment limit for older running sessions. Create a fresh terminal
  after rebuilding to exercise snapshot attachment.
- No disk recovery after server exit. Closing a window preserves a process;
  restarting the server does not.
- The runtime answers cursor/size/device/version queries while detached. Attached
  viewers answer live queries; snapshot decoding emits no historical replies.
  Detached appearance/pixel metrics and graphics restoration remain incomplete.
- Raylib's input and font rendering limitations apply. IME,
  font fallback/shaping, and full Kitty keyboard behavior are not validated.
  This spike does not choose Raylib as the final UI toolkit.
- The workspace UI is a layout experiment, capped at eight workspaces and eight
  terminals per workspace. Saved moves/reordering and optional hook-driven agent
  status, continuous horizontal navigation and scoped theming are implemented.
  Configurable navigation shortcuts, visual-layout persistence and packaging
  remain planned.
  See [the roadmap](../../docs/roadmap.md) and [UX spike](../../docs/ux-spike.md).

## Selection and clipboard

Drag to select, double-click a word, or triple-click a line. Selection highlighting
and plain-text extraction use Ghostty VT, including soft-wrap unwrapping.
Copy/Paste buttons expose the actions; Linux uses Ctrl+Shift+C/V and macOS uses
Command+C/V (not yet tested on a Mac). Ctrl-C still interrupts terminal programs.
Hold Shift to select within mouse-reporting apps. Mouse wheel and scrollbar
inspect history; typing or pasting returns to live output.

Paste uses Ghostty's paste encoder, including bracketed paste when requested by
the application. Copy and paste each have a 64 KiB limit, with a header notice
when exceeded. Edge-drag autoscroll and selection persistence across viewer
reattachment are not implemented. The Linux GUI smoke check uses xclip on its
isolated display to verify real clipboard transfer.

## Terminal overview

Overview in the header or Ctrl+Shift+O on Linux opens a searchable list of every
terminal, including those outside the visible pane columns. Command+Option+O is
the unverified macOS binding. Filter by workspace or terminal name, click a row,
or use Up/Down and Enter. Escape/Done dismisses; wheel scrolling reveals longer
lists. Filtering supports ASCII case-insensitive matching and exact UTF-8 bytes.
The list reads client connection/unseen-output flags and optional runtime agent
identity. Agent names appear in pane titles and beneath overview rows; Agents
restricts the list to identified foreground agents. Search also matches agent names.
Disconnected does not prove a shell exited, and the marks are not agent attention
detection. Older running servers need a shell relaunch to publish identity. See
[foreground identification limits](../../docs/agent-identification.md).
All sessions continue parsing while open. Search and navigation input never reaches
the shell. This client presentation does not change the runtime protocol.

## Workspace layout

`just spike` initially opens Main and Scratch with two shells each, then reopens
your saved organization on subsequent launches. The sidebar has New workspace,
Rename workspace, and New terminal controls; titles have Rename controls. A name
dialog supports Enter to save, Escape/Cancel, select-all, Backspace, and desktop
clipboard paste (Ctrl+V on Linux; Command+V on macOS, unverified). Names are
single-line UTF-8 strings of at most 63 bytes. New workspaces start with one shell.
Renaming preserves the session and running process. Prev/Next focuses terminals
and reveals those outside the visible variable-width strip. Click a numbered position to
jump, or scroll over the top navigation strip. Pane slides retain immediate focus
and typing; reduced motion skips them. Terminal-content scrolling is unchanged.
`just navigation-gui-test` validates this with one/fifteen populated terminals.
Scroll the workspace list if
the window is too short to show every row.

Click a pane/title to focus it. Each terminal starts at half width. Ctrl+Shift+R
(Linux) / Command+Option+R (Mac) cycles half → two thirds → third; Ctrl+Shift+F /
Command+Option+F toggles full terminal-area width and restores the preset on a second
press. Drag a right gap for a snapped guide and release to apply that terminal's
width. Commands also offers explicit presets. Widths and strip positions are
saved independently in each window view.
The blue border marks selection; global Copy/Paste follows it. Gold dots mark live
output in unfocused terminals. Closing the window detaches all sessions; restarting
the launcher reconnects them. Typing `exit` closes only that shell. Renaming leaves
ended shells ended; rerunning the launcher starts replacements for missing sessions.

Terminal Stop and the x beside WORKSPACES open a confirmation. Stop terminates the
named shell(s) and their foreground jobs and removes their saved entries; jobs that
have detached from the terminal are outside this guarantee. Cancel/Escape preserves
the sessions; Enter does not confirm. Empty workspaces support New terminal, and an
empty catalog supports New workspace. Removed terminals are not recreated on reopen.
Earlier live spike servers need their shells exited and the launcher reopened to
gain the new stop endpoint. The helper records close intent before stopping, retries
an interrupted close before any startup, and preserves metadata if a stop fails.

Organization lives in `.local/sessions/workspaces.json`. The old fixed TSV catalog
is migrated without changing socket identities. A generated version-3 TSV projection
has separate workspace and terminal records, including empty workspaces. JSON is
the source of truth; do not edit the generated projection. Invalid/unsupported JSON
and existing sockets are preserved on startup errors.

`WORKBENCH_WORKSPACES` selects workspace mode; `WORKBENCH_SOCKET` without a catalog
retains single-terminal diagnostics. The management controls require the environment
provided by `scripts/run-spike.py`; diagnostics using a hand-written legacy catalog
disable them. `just workspace-gui-test` checks the layout and
`just management-gui-test` checks creation, renaming, and saved organization on an
isolated Xvfb display. Management is available independently through
`python3 scripts/workspace_catalog.py --help`.
The terminal setup derives from Ghostling; [LICENSE.ghostling](LICENSE.ghostling)
preserves its MIT copyright/permission notice beside the source adaptations.

Show shortcuts in Commands, Ctrl+Shift+/ (Linux), or Command+Option+/ (Mac) opens
a grouped help overlay using the current bindings. It works over Overview too.
Escape/Done/toggle close it; Enter/Edit shortcuts opens settings. Mac defaults for
Commands, Overview and new terminal/workspace now use Command+Option+P/O/T/N.
Saved custom bindings keep their previous chords until edited or reset with Default.
