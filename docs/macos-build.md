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
Also exercise Command+Shift+O, Option/Alt, composition, keyboard layouts and
reduced motion (`WORKBENCH_REDUCED_MOTION=1 just spike`).

If a build or launch fails, keep the command and its error output. Runtime logs
live under `.local/sessions`. Build/runtime caches and logs stay ignored by Git.
See [the platform acceptance checks](first-spike.md) and [roadmap](roadmap.md).

## Startup diagnostics

Every viewer launch appends stdout, stderr, a timestamp and the exit code to
`.local/sessions/viewer.log`, while also showing its output in the terminal.
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

Only one viewer can attach to a session at a time. Running `git pull && just spike`
inside an already open Workbench launches a second viewer; it cannot take over
the first window's sessions. Close the first window, then run `just spike` from
macOS Terminal to test reconnection. Closing the window detaches its sessions;
using `exit` or Stop ends a session. Busy sessions display "Open in another window"
and remain usable in the window that owns them.
