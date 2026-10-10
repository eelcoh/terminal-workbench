# Terminal Workbench

A desktop terminal workspace for running and overseeing coding agents.

Working name. The repository now includes a runnable Linux terminal integration
spike with workspaces, a searchable overview, foreground agent identification,
and optional lifecycle status.
Real interactive agent-hook compatibility remains unverified.

## Product direction

- macOS and Linux are first-class targets.
- A restrained interface inspired by Foot, with visual and interaction quality approaching Dank Linux.
- Ghostty-compatible terminal themes with matching application colors.
- Workspaces, horizontally scrolling terminal panes, and an overview of agents across workspaces.
- Clear indicators for agents working or awaiting input, with direct navigation to their terminal.
- Persistent sessions owned by a background runtime, independent of open windows.
- A fresh, configurable keyboard model with platform conventions and explicit terminal input routing.

The [roadmap](docs/roadmap.md) includes a dedicated [UX spike](docs/ux-spike.md),
platform validation, theming, runtime hardening and installable releases.

This project follows an experiment built on Herdr. It has independent Git history
and contains no copied Herdr code.

## Build on a Mac

Clone this repository on your Mac and follow [the native build guide](docs/macos-build.md).
It covers Apple Silicon/Intel dependencies, the pinned Zig compiler, build/run
commands and platform validation. The owner has confirmed a native build/run;
full input, clipboard and interaction acceptance remains open.

## Try the terminal spike

On this workstation:

```sh
cd /var/home/eelco/Code/ideas/terminal-workbench
distrobox enter build-machine -- just spike
```

The first build downloads pinned Ghostling, Ghostty VT, and Raylib dependencies.
The first launch starts **Main** and **Scratch**, each with two independent shells.
Later launches reopen your saved workspaces and terminal names.
Click a workspace in the sidebar, then click a pane or its title to focus it.
The blue border marks the selected pane. Drag the gap between panes to resize.
Copy/Paste targets the selected pane. Gold dots mark output from an unfocused
terminal; they are activity indicators, not agent status.

Use **New workspace**, **Rename workspace**, and **New terminal** at the bottom
of the sidebar. Each terminal title has a **Rename** button. New workspaces start
with one shell. **Prev/Next** in the header focuses terminals and reveals those
outside the two visible columns. Click a numbered position to jump directly, or
scroll over the header's navigation strip to move between terminals. Horizontal
trackpad scrolling and vertical mouse-wheel scrolling work there; scrolling inside
terminals keeps its terminal behavior. Adjacent pane changes scroll for 270ms; longer jumps scroll through every
intermediate pane, taking up to 1.25s depending on distance, with immediate
focus and typing. `WORKBENCH_REDUCED_MOTION=1 just spike` skips motion.
Names are saved; renaming preserves the process.

Each terminal has **Stop**; the **x** beside WORKSPACES stops the selected workspace.
The confirmation names the target and shows the number of affected shells.
Click **Stop** to terminate those shells and their foreground jobs and remove them
from the saved organization. Escape/Cancel keeps them running. Empty workspaces
remain available for **New terminal**; closing every workspace leaves **New workspace**.
Stopped terminals stay removed when reopening. Jobs detached from the terminal
are outside this stop operation.

Sessions started by earlier spike binaries lack the stop endpoint. Their close
dialog asks you to exit their shells and reopen before using Stop; rebuilding
does not replace running servers.

Use **Overview** in the header or **Ctrl+Shift+O** on Linux to see terminals
across all workspaces. The default **Spatial** view groups live terminal previews
into workspace rows, with a short zoom transition when entering or leaving.
**Left/Right** selects a terminal; **Up/Down** selects a
workspace; **Enter** or a thumbnail click enters it. Scroll vertically through
workspaces; **Shift+scroll** or the row arrows reveals more terminals horizontally.
Type to filter names. **List** keeps the compact searchable list available.
Drag a thumbnail into another workspace or between cards to move it. Empty
workspaces stay visible as drop targets. Escape cancels a drag; releasing over a
valid target saves placement while the session keeps running.
Escape/Done dismisses either view. [Preview details](docs/spatial-overview.md).
Connection labels and unseen-output dots stay live while the overview is open;
they do not indicate whether an agent is working or waiting. Recognized agent names
appear beside workspace labels and in terminal titles. **Agents** filters the list
to identified foreground agents in either view; search also matches their names. Identification
updates within roughly one second, including while detached. Older live sessions
need their shells exited and the launcher reopened to gain identification. The macOS binding
is Command+Shift+O, pending Mac validation.

In a **new terminal**, use `wb-codex` or `wb-claude` to launch with lifecycle
hooks. Titles and overview rows show **Working**, **Needs input**, or **Ready**;
**Needs input** filters the overview to agents waiting for you. Plain `codex` /
`claude` and other agents keep **Unknown** unless integrated. Codex may require
reviewing the Workbench hooks in `/hooks`. The launchers add settings for that
invocation; they do not edit global agent configuration. Existing shells need a
new terminal for the updated runtime and PATH. See [hook details](docs/agent-attention.md).

Use **Appearance** in the sidebar or **Theme** in the overview to browse palettes.
Choose **Application**, **Workspace**, **Terminal**, or **Defaults**. Terminal overrides
inherit from workspace colors when cleared; workspace colors inherit from Defaults.
Chrome stays stable by default; **Follow active workspace** enables matching chrome.
Apply saves all pending changes; Cancel restores the whole dialog. The offline
gallery includes Dracula and all four Catppuccin variants. Import Ghostty color
fields with `WORKBENCH_THEME=/path/to/theme just spike` to set terminal defaults.
See [appearance support and limits](docs/appearance.md).

Click **Close** in the top-right header to detach all shells, then run the
command again to reconnect to the same processes. The original spike shell is
kept as Main / Shell 1. Type `exit` to stop only the selected shell. Closing an
older single-terminal window before relaunching releases its attachment.

Drag to select text, double-click a word, and use **Copy** / **Paste** in the
header or **Ctrl+Shift+C/V** on Linux. Scroll with the mouse wheel; typing or
pasting returns to live output. Hold Shift to select inside mouse-enabled apps.

The spike uses Linux X11/XWayland. Automated checks use an isolated Xvfb display.
Full macOS interaction acceptance and native Wayland remain unverified. This is an integration experiment
with bounded scrollback and incomplete desktop input support, not a daily terminal.
See [the spike's usage and limits](spikes/ghostling/README.md).

New runtimes retain VT state and answer terminal queries while detached. The
`read` and `snapshot` CLI commands inspect this state without opening a viewer;
see [runtime hardening](docs/runtime-hardening.md). New windows reconnect through
terminal snapshots, so new runtimes keep accepting attachments beyond 16 MiB of
output. Older live runtimes use replay fallback and retain their existing limit.

## Layout

```text
crates/runtime/          Shared Rust session runtime and workbench-session executable
spikes/ghostling/        Viewer adapter for the pinned official Ghostling example
scripts/                Source preparation and local spike launcher
tests/                  Real PTY and isolated graphical smoke checks
docs/architecture.md    Ownership and platform direction
docs/keyboard.md        Fresh keyboard interaction requirements
docs/first-spike.md     Milestone acceptance checks
docs/integration-findings.md  Source investigation, results, and open decisions
```

## Development

Use a stable Rust toolchain, `just`, Python, Git, a C compiler and Zig 0.16.0.
`just runtime-build` builds the runtime without graphics dependencies; `just check`
prepares its pinned VT library before validation. Renderer builds additionally
need CMake, Ninja, a C++ compiler and platform graphics development dependencies.
On this workstation those tools live in the `build-machine` Distrobox.
`just spike-build` automatically discovers Zig 0.16.0 in `.local/tools` or PATH.
Use `ZIG=/absolute/path/to/zig just spike` to select another installation; the
wrapper sets the build environment without changing your shell settings.

From the project directory:

```sh
distrobox enter build-machine -- just check
distrobox enter build-machine -- just spike-gui-test
distrobox enter build-machine -- just workspace-gui-test
distrobox enter build-machine -- just navigation-gui-test
distrobox enter build-machine -- just management-gui-test
distrobox enter build-machine -- just theme-gui-test
```

`just check` runs Rust formatting, Clippy, unit tests, real PTY/stop smoke checks, and catalog persistence/identity/recovery checks.
The graphical check additionally requires Linux, Xvfb, xdotool, xclip, and vi.

Source is hosted at [eelcoh/terminal-workbench](https://github.com/eelcoh/terminal-workbench).
Workbench code uses the [MIT license](LICENSE). Upstream components retain their
own licenses; the source preparation script preserves Ghostling and font notices;
[the viewer notes](spikes/ghostling/README.md) record dependency revisions.
