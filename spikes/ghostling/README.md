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
  A second viewer of the same session receives a rejection.
- A 16 MiB in-memory output/resize journal. Reconnection is disabled after the
  cap; live output continues. A final runtime needs authoritative terminal state
  and bounded state snapshots, not an indefinitely growing output replay.
- No disk recovery after server exit. Closing a window preserves a process;
  restarting the server does not.
- Query replies are suppressed during replay. While detached, queries have no
  viewer to answer them; programs that need these replies may stall. Server-owned
  VT state is needed before detached agent execution is production-ready.
- Raylib's input and font rendering limitations apply. IME,
  font fallback/shaping, and full Kitty keyboard behavior are not validated.
  This spike does not choose Raylib as the final UI toolkit.
- The workspace UI is a layout experiment, capped at eight workspaces and eight
  terminals per workspace. Saved moves/reordering and optional hook-driven agent
  status are implemented. Smooth horizontal scrolling, configurable navigation
  shortcuts, visual-layout persistence, theming and packaging remain planned.
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
terminal, including those outside the visible pane columns. Command+Shift+O is
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
and reveals those outside the two visible columns. Click a numbered position to
jump, or scroll over the top navigation strip. Pane slides retain immediate focus
and typing; reduced motion skips them. Terminal-content scrolling is unchanged.
`just navigation-gui-test` validates this with one/fifteen populated terminals.
Scroll the workspace list if
the window is too short to show every row.

Click a pane/title to focus it and drag the divider to resize two visible panes.
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
