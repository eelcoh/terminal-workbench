# Build and run on macOS

This is the development prototype, not an installed `.app` bundle. The renderer
and macOS input/process adapters need native validation. Linux test results do
not establish Mac behavior; record failures as part of the platform spike.

## Prerequisites

Install Apple's Command Line Tools if they are not installed, and finish their
installer before building:

```sh
xcode-select --install
```

With [Homebrew](https://docs.brew.sh/Installation) available:

```sh
brew install cmake ninja just python gh
```

Use Python 3.11 or newer. Install Rust through rustup if it is missing. One
Homebrew option is:

```sh
brew install rustup
export PATH="$(brew --prefix rustup)/bin:$PATH"
rustup toolchain install stable --profile minimal --component rustfmt --component clippy
```

Homebrew's [rustup formula](https://formulae.brew.sh/formula/rustup) is keg-only;
keep its bin directory in your shell's PATH when using this installation. An
existing rustup installation can be used instead. The repository's
`rust-toolchain.toml` selects stable and its required components.

## Clone

Configure a GitHub SSH key on the Mac, then clone into a short local path,
for example `~/Code/terminal-workbench`. GitHub CLI can help configure access:

```sh
gh auth login --git-protocol ssh
git clone git@github.com:eelcoh/terminal-workbench.git
cd terminal-workbench
```

A short checkout path matters because session Unix sockets currently live inside
`.local/sessions`, and macOS Unix socket paths have a small fixed limit. The
repository does not contain Linux sessions, user settings or downloaded dependencies.
New terminal socket filenames use 16-character IDs, so the absolute session directory
must fit 81 UTF-8 bytes (plus `/`, the ID, and `.sock` within 103 bytes). Existing
terminal IDs and socket paths are preserved; older 32-character IDs need more room.

## Install the pinned Zig compiler locally

The pinned Ghostty revision requires Zig 0.16.0. A newer package-manager version
is not a substitute. If `zig version` already reports `0.16.0`, skip this section.
Otherwise, run these commands from the repository directory. The checksums come
from the [official Zig download index](https://ziglang.org/download/index.json).

```sh
case "$(uname -m)" in
  arm64)
    wb_zig_arch=aarch64
    wb_zig_sha=b23d70deaa879b5c2d486ed3316f7eaa53e84acf6fc9cc747de152450d401489
    ;;
  x86_64)
    wb_zig_arch=x86_64
    wb_zig_sha=0387557ed1877bc6a2e1802c8391953baddba76081876301c522f52977b52ba7
    ;;
  *) echo "Unsupported Mac architecture"; return 1 ;;
esac
mkdir -p .local/tools
wb_zig_name="zig-${wb_zig_arch}-macos-0.16.0"
wb_zig_archive=".local/tools/${wb_zig_name}.tar.xz"
curl --fail --location "https://ziglang.org/download/0.16.0/${wb_zig_name}.tar.xz" -o "$wb_zig_archive"
printf '%s  %s\n' "$wb_zig_sha" "$wb_zig_archive" | shasum -a 256 -c -
```

Continue only if the checksum reports `OK`:

```sh
tar -xf "$wb_zig_archive" -C .local/tools
".local/tools/$wb_zig_name/zig" version
```

The build wrapper discovers this repo-local Zig automatically; no PATH export is
needed for later builds. On Apple Silicon, use a native terminal/toolchain
consistently; do not mix Rosetta and native build caches.

## Build and run

```sh
just spike-build
just spike
```

`just spike-build` and `just spike` use `scripts/build-spike.py`. It finds Zig 0.16.0
in the native `.local/tools/zig-ARCH-macos-0.16.0` directory, `.local/tools/zig`, or
PATH. For another installation, use `ZIG=/absolute/path/to/zig just spike`.
An explicit ZIG override must be valid; it never silently selects another compiler.
The wrapper sets PATH and ZIG for build subprocesses and passes the selected path
to CMake, replacing any old cached Zig location. Your shell environment is unchanged.
You can also build directly with `python3 scripts/build-spike.py`.

The first build downloads the pinned Ghostling, Ghostty VT and Raylib sources and
builds them natively. All GitHub dependency fetches use SSH, including Raylib;
the Mac needs a working GitHub SSH key (`ssh -T git@github.com`). Existing
Ghostling dependency checkouts are migrated to the SSH origin automatically.
No installed Ghostty app, Distrobox, X11 or Linux library
packages are required for the Mac build. CMake links the Mac system frameworks.
The first run starts Main and Scratch with two shells each. Later runs reuse your
Mac's saved organization and live sessions. Close detaches; Stop terminates the
chosen session after confirmation. Use the same checkout for later launches.

```sh
git pull --ff-only
just spike
```

`just spike` includes the build step. Updating source does not update already
running runtime processes; existing sessions keep their original capabilities.
To exercise runtime changes, explicitly stop the relevant test shells and start
new terminals. Closing the window alone keeps those processes alive.

## Native checks

Basic source checks can run on the Mac:

```sh
cargo fmt --all -- --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace
python3 tests/catalog_test.py
python3 tests/move_test.py
```

The full `just check` and GUI recipes currently include Linux-specific fixtures
and Xvfb/xdotool tests; they are not yet a complete Mac test suite. Manually check
shell input and Ctrl-C, a full-screen editor, resizing, selection/scrollback,
Command+C/V, overview navigation/dragging, and Close/reconnect to the same shell.
Also exercise Command+Option+O, Option/Alt, composition, keyboard layouts and
reduced motion (`WORKBENCH_REDUCED_MOTION=1 just spike`).

If a build or launch fails, keep the command and its error output. Runtime logs
live under `.local/sessions`. Build/runtime caches and logs stay ignored by Git.
See [the platform acceptance checks](first-spike.md) and [roadmap](roadmap.md).

## Startup diagnostics

Every viewer launch appends stdout, stderr, a timestamp and process identity to
`.local/sessions/viewer.log`. Launches from an external terminal also show the
output there and record the exit code. Launches from inside Workbench start the
viewer independently, returning the invoking shell promptly. Its later output
stays in the log; immediate startup failures still report their exit status.
Shell runtime logs remain separate (`.local/sessions/TERMINAL-ID.log`); they
need not change when attaching to an existing session. If `just spike` exits
with code 1, inspect the viewer log first. Raylib's “Window closed successfully”
message reports cleanup and can appear after a startup or attachment failure.
Do not delete saved sockets to diagnose this: the log identifies a failed
attachment's socket path, and existing sessions are preserved.

If startup reports a socket with no listening server (for example after a reboot),
run `just recover-sessions`, then `just spike`. Recovery probes both the terminal
and control sockets and preserves any session with a listening endpoint. It only
removes owned, dead socket files; other files and symlinks are rejected. Dead
sessions start fresh shells with their saved names and IDs. Their previous shell
process and in-memory output cannot be restored. Workspace organization and
appearance preferences are retained. Normal startup never deletes socket files.

## Runtime terminal-state build

`just runtime-build` prepares the pinned libghostty-vt and Rust runtime without
building Raylib or requiring a graphical desktop. It uses the same Zig discovery
and SSH preference as `just spike`. `just check` now prepares that library before
Rust validation. New terminal-state/query behavior needs a newly created terminal;
rebuilding preserves older running session processes. See
[the runtime slice and native validation status](runtime-hardening.md).

Snapshot-based reattachment is now implemented for newly created runtimes. After
building, create a fresh terminal, leave output/application state in it, close the
window and run `just spike` again. The same process should resume with its text
and terminal modes. Older live runtimes use replay fallback. Linux Xvfb checks
cover this path and long-output recovery; native Mac acceptance remains pending.

Only one window owns a terminal at a time. Running `git pull && just spike`
inside Workbench starts an independent second window and returns the invoking
shell prompt. Busy terminals display "Open in another window"; newly created
runtimes support explicit Move here. Older runtimes cannot transfer ownership:
a rejected transfer preserves the current owner, and the action becomes
Reconnect. Close its owning window first, then reconnect from the other one.
Closing a window detaches its sessions; `exit` or Stop ends a session.

With a launcher from before this fix, a second window may still be a foreground
job in the terminal that launched it. Closing that second window releases the
launcher and returns the original shell prompt. Existing live runtimes remain
unchanged by rebuilding.

Overview trackpad scrolling now uses fractional movement and short easing, with
horizontal swipes to pan a workspace's terminals. Test gentle movements, longer
swipes, reversal, and scrolling at both ends with a populated overview. Reduced
motion keeps fractional movement but removes easing. Native acceptance remains
pending; Linux validates the callback and populated GUI paths.

### Shared windows and terminal transfer

Open two instances against the same state directory. Create or rename a workspace
in one and check that the other updates without relaunching. New terminals should
stay with their creating window. In the other window, enter that terminal from
the overview and click **Move here**. Check that shell variables and running jobs
survive, then move it back. Selection and focus in unrelated workspaces should
remain local to each window. Closing a workspace should remove it from both.

Direct transfer requires a runtime launched by this build. Existing terminals
keep their earlier runtime: the UI will ask you to close their owning window
before clicking Reconnect. This reconnects without restarting the shell.

## Session service acceptance

After `git pull && just spike`, open a second window and verify that both launches
print the same Session service PID. `just session-status` reports runtime health;
already-running older runtimes may be shown as unadvertised or legacy replay.
Run `just service-stop` and verify that typing still works in an existing window.
Launching another window should start a new service and retain those shell processes.
For a disposable terminal, run `exit`, then launch another window: its saved entry
should remain stopped. `just recover-terminal ID` explicitly starts a new shell for
that entry while preserving its name and theme. This service slice is validated on
Linux. Native macOS startup/reuse, shared PID and stop/live-adoption were confirmed
on 2026-10-11; selected stopped-terminal recovery still needs acceptance.
See [session service](session-service.md).

## Window view restoration acceptance

Run `just spike` twice and note each printed Window view slot. Choose a different
workspace and focused terminal in each, then resize and adjust the divider. Close
one window and launch `just spike` again: it should reuse that slot and restore
its size and working view while the other window keeps its own focus. Open the
overview, scroll both axes and close/reopen to check its saved context. Ownership
stays explicit: terminals owned by another window still offer Move here.
The owner confirmed native view restoration on 2026-10-11. See [window state](window-state.md).

## Graphical stopped-terminal recovery acceptance

In a disposable terminal, run `exit`. It should keep its frozen output and show
Shell stopped with **Start new shell** in the header. Open the confirmation and
Cancel: no shell should start. Confirm on a second attempt: a fresh shell should
accept input with the same terminal name and theme. An entry owned by another
window must continue offering Move here, never Start new shell. Native Mac
acceptance of this recovery flow was confirmed by the owner on 2026-10-11. Detailed exit causes remain
unknown after service restart; stopped is the supported fact.

## Workspace folder acceptance

After updating, use New workspace and enter a project folder (Tab moves from the
name to the folder). Run `pwd` in its first shell. Use New terminal and confirm
it starts in the same folder. Change the project folder using Folder in the sidebar: existing
shells must retain their current directory; a new terminal should use the new
folder. Run `exit` in a disposable terminal, then Start new shell: its replacement
should use the current workspace folder. Try a path with spaces and a missing
folder. The owner confirmed workspace creation, releasing capacity by closing one
of eight workspaces, folder editing and new-terminal startup in the chosen folder.
Recovery-directory and invalid-path checks remain pending. After the UX update,
verify the Workspace limit (8) label and hover explanation at capacity, and the
selected-path highlight and replacement hint in Folder, with light and dark themes.
See [project folders](workspace-directories.md).

## Commands palette acceptance

After updating, click Commands at the bottom of the sidebar or press
Command+Option+P. Search for New workspace, Rename terminal, project folder,
Next workspace and Change appearance. Verify Enter opens an editor without saving
it, and that Stop terminal only opens a confirmation; Enter must not stop it.
Escape should return cleanly to the same shell. At capacity, search New workspace
and verify the explanation. Try an empty workspace and the smallest window.
Command+A/V should edit the query, and ordinary Ctrl-C/D/Z should still reach the
shell outside the palette. Native acceptance of these new palette checks is pending.
See [Commands](commands.md).

## Configurable shortcut acceptance

Use Command+Option+Left/Right to navigate terminals and Command+Option+Up/Down to
navigate workspaces. Command+Option+T/N should open terminal/workspace creation without
saving; Escape cancels. In Commands, search Shortcut settings, then Next terminal.
Record another chord and Save. Verify it appears in Commands, works, survives a
window reopen, and can be cleared or restored with Default. A duplicate chord must
show a conflict without changing the saved map. Try navigation while a name dialog
is open: it must stay in that dialog. Stop shortcuts must still require clicking
confirmation.

In a disposable terminal application, choose Send next key to terminal and press
a bound chord. Verify the application receives it and Workbench focus stays put;
Escape or clicking elsewhere cancels an armed forward. Clipboard shortcuts can be
cleared without leaving their old interception active. Bare Ctrl-C/D/Z stay terminal
input. Mac settings use `.local/shortcuts-macos.conf`; Linux preferences are separate.
These new native checks remain pending. See [shortcuts](shortcuts.md).

## Shortcut-help acceptance

Open help with Command+Option+/ in the working view and in Overview. Confirm it
shows your actual bindings, including customized or cleared actions. New Mac
defaults use Command+Option+P/O/T/N for Commands, Overview and terminal/workspace
creation; copy/paste stay Command+C/V. If earlier defaults were saved as explicit
bindings, reset those actions using Default in their shortcut recorder.

Close help with its chord, Escape or Done, preserving the underlying shell focus
or Overview query/selection. Enter/Edit shortcuts should open settings without
editing an action in the same frame. Record a slash chord, test conflicts and
clear/remap help; Commands must remain a mouse route to Show shortcuts.

Check slash/Option on your keyboard layout, and ensure the chord or its generated
text never reaches the shell while help is open. Resize while help is open, scroll
the narrow list with keyboard and trackpad, and review dark/light readability.
Live output should continue parsing, shell variables/PIDs should survive, and
normal terminal sizing resumes when returning to the working view. Native results
for this slice are pending.

### Keyboard focus acceptance

Run `just spike`, open Commands (Command+Option+P), and use Tab/Shift+Tab to
reach search, results and Done. Test Enter/Space on buttons. In New workspace,
visit name, folder, Cancel and Create. In Stop terminal or Start new shell,
initial Enter/Space must do nothing; Tab selects Cancel, then the confirmation.
Open help with Command+Option+/, focus Edit shortcuts or Done, and verify Escape
returns to the previous Overview filter and focus. In Appearance, visit search, gallery, close,
filters, reset and Cancel/Apply; Escape must restore prior colors.
Native focus and keyboard-layout acceptance of this slice is pending.

### Typography and Retina review

Rebuild with `just spike` and compare the terminal's text sharpness with the
previous build on the same Retina display. Terminal font and logical size remain
the same; the backing canvas now renders at display resolution. UI labels use a
bundled sans-serif face with larger titles and more readable captions.

Review Commands, help, name/path dialogs, Appearance and Overview in both dark
and light application themes. Check long names/paths, narrow-window clipping,
keyboard focus and hovered/pressed/disabled controls. Try mixed third/two-third
widths and full width; selecting/copying text should still follow the same grid.
Also review a non-Retina display and moving the window between different-DPI
monitors. This slice's native typography/sharpness acceptance is pending.

### Pane-header and sidebar review

Rebuild with `just spike`. Check the active workspace marker, terminal counts and
unread dots, then focus neighboring terminals with Command+Option+Left/Right.
The active pane should have a quiet header tint and marker; Stop/Rename should
still be visible and show hover/pressed feedback.

Try long terminal/workspace names, mixed thirds/two-thirds widths and a narrow
window. Agent identity and Working/Ready/Needs input should read separately from
the name; long names should end cleanly with an ellipsis. Move here, reconnect
and stopped-terminal recovery should stay in the header with their existing
confirmation behavior. Native acceptance of this refinement is pending.

### Mixed-display and swipe review

After pulling/building this slice:

1. Start on the MacBook with no external screen. Check text and all four active
   pane edges. Record `stty size`, `echo $$` and a shell variable in a spare pane.
2. Connect the regular external monitor and move the same window onto it. Text
   should use that screen's backing density and all pane borders should remain
   visible. A density-only move at the same logical size should retain the grid
   and shell PID/state; a real window resize may change `stty size`.
3. Move back to Retina, then move to the external screen and unplug it with the
   app still running. Content should retain its normal logical size, and an
   oversized window should fit the MacBook's usable area. Check dialogs/help,
   terminal input, pane selection and the original shell variable/PID.
4. Repeat with Appearance or shortcut help open, then close it. Confirm the
   working terminals return with sharp text and usable input.
5. Swipe left/right with two fingers over terminal content. The strip should pan
   continuously, including partial panes. Vertical scrolling still goes to the
   terminal. Existing pane navigation chords and position marks remain available.

Density transitions write `Display backing scale updated: ...; logical cells ...
retained` to `viewer.log`. These logs and the live Linux callback/geometry checks
support diagnosis; native Mac hardware acceptance remains pending.


### Workspace-first theme review

Open Change appearance in a workspace. Confirm its name in the title, search
Dawn/Dracula and preview: inheriting terminals and chrome should change while
the picker surface stays steady. Cancel restores both. Apply to workspace saves
only that workspace's palette, retaining existing preferences and live shells.
Repeat from Overview, with an empty workspace, and in a narrow window.

Use a pane's Terminal theme menu for an exception. The workspace picker should
report it and preserve its colors; Use workspace clears it only on Apply.
Verify reopening, Tab/Shift+Tab, Enter/Escape, dotfile defaults and explicitly
fixed chrome (`follow-workspace = false`). Native acceptance is pending.

## Standalone viewer menu name

The prototype sets its Darwin program name to **Terminal Workbench** before
GLFW creates the Cocoa menu, replacing the inherited `ghostling` menu label.
The existing build/launcher executable path stays compatible. This uses
[Darwin's setprogname](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/setprogname.3.html)
and a process-lifetime string. Linux builds compile a no-op platform adapter.
A mocked adapter contract is included in `just check`; native Mac review is pending.


## Unexpected viewer disappearance

Foreground launches observe the viewer process independently of its output pipe.
A child retaining that pipe cannot prevent exit reporting. `viewer.log` records
exit status, PID, UTC timestamp and a signal name for signal termination. Ctrl-C
records whether the viewer has exited or is still running instead of printing a
launcher traceback. This does not stop runtime shells. Detached launchers only
observe the short startup check; later detached exits still need native reports.

The October 17 overnight upload shows a 21:55 UTC launch without a recorded exit
before the next 09:05 UTC launch. It contains no shutdown sequence or native
crash report, so the cause of that disappearance remains unconfirmed. The pipe
case is reproduced by the launcher regression test, not established as the Mac
failure. For a recurrence, preserve `.local/sessions/viewer.log` and any macOS
Ghostling / Terminal Workbench crash report.
