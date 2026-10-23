# Build and run on macOS

This builds the Qt client and Rust runtime from a checkout. Linux test results do
not establish Mac behavior; record Mac results separately.

## Prerequisites

Install Apple's Command Line Tools if they are not installed, and finish their
installer before building:

```sh
xcode-select --install
```

With [Homebrew](https://docs.brew.sh/Installation) available:

```sh
brew install cmake ninja just python gh qt
```

Use Qt 6.8 or newer; set `CMAKE_PREFIX_PATH` to its prefix if CMake cannot find
it. Python 3.11 or newer is needed for development scripts and tests, not by
opening an installed preview. Install Rust through rustup if it is missing. One
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

A short checkout path matters because development session Unix sockets live inside
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
just toolkit-workbench
```

This prepares the pinned libghostty-vt with Zig, builds `workbench-session` and
the Qt client, and opens your saved development workspaces from `.local/sessions`.
Closing the window detaches; shells keep running. The first build clones the
pinned Ghostty revision over SSH, so the Mac needs a working GitHub SSH key
(`ssh -T git@github.com`). No installed Ghostty app is required.

`scripts/build-runtime.py` finds Zig 0.16.0 in the native
`.local/tools/zig-ARCH-macos-0.16.0` directory, `.local/tools/zig`, or PATH. For
another installation, use `ZIG=/absolute/path/to/zig just toolkit-workbench`. An
explicit override must be valid; it never silently selects another compiler.

To install a preview app outside the checkout:

```sh
just toolkit-install
```

Then close preview windows, run the printed `--activate` command and open the
app it names. See [the staged preview](qt-staged-preview.md).

Updating source does not update already running runtime processes. Close the
window and start new terminals to exercise runtime changes.

## Native checks

```sh
just check
just toolkit-search-test
just toolkit-scroll-test
just toolkit-shortcuts-test
just toolkit-workspace-appearance-test
```

The graphical `toolkit-*-gui-test` and `toolkit-ui-test` recipes use Xvfb,
xdotool or a nested Wayland compositor and are Linux-only. On the Mac check by
hand: shell input and Ctrl-C, a full-screen editor, resizing, selection and
scrollback, Command+C/V, mouse reporting in a mouse-enabled app, Overview, IME
composition, trackpad scrolling, and Close/reopen to the same shell.

## Diagnostics

Installed previews log to `launch.log` and `service.log` in
`~/.local/state/terminal-workbench/qt-preview/`; development runs use
`.local/sessions`. Shell runtime logs are `TERMINAL-ID.log` in the same
directory. Do not delete sockets to diagnose a failure: the logs name the
failing socket, and existing sessions are preserved.

After a reboot, `just recover-sessions` starts fresh shells for saved terminals
whose sockets have no listener, keeping their names, IDs and appearance. It only
removes owned, dead socket files.
