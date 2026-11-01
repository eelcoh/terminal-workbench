# Band release artifacts

Band is pre-release. The artifacts are unsigned (macOS: ad-hoc signed, not
notarized) and have no updater. They need no Python.

## Build

```sh
just package          # native build for this machine
```

| Platform | Artifact | Contents |
| --- | --- | --- |
| macOS | `dist/Band-<version>-macos-<arch>.zip` | `Band.app` with Qt deployed inside |
| Linux | `dist/band-<version>-linux-<arch>.tar.gz` | `band-<version>-linux-<arch>/`, uses the system Qt 6.8+ |

On this Linux workstation, run it inside the `build-machine` Distrobox. On
Fedora 44 (the Distrobox and the Linux CI container) the build needs:

```sh
sudo dnf install gcc-c++ cmake ninja-build just cargo rust clippy rustfmt zig python3 \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg git-core openssh-clients \
  zsh procps-ng findutils which diffutils xz tar fontconfig
```

`.github/workflows/linux.yml` builds, tests and packages in that container as
an unprivileged user, and uploads the archive as `band-linux-x86_64`. A Mac
build needs Homebrew Qt (`brew install qt`), Zig 0.16.0 and Rust; see
[the macOS build guide](macos-build.md). `just package-test` checks the layout,
relocation, agent launchers, saved sessions and an offscreen Qt start (Linux).

## Install

**macOS.** Unzip and move `Band.app` to `/Applications` (or `~/Applications`).
The app is ad-hoc signed, not notarized, so a browser download is quarantined
and Gatekeeper refuses it. Since macOS 15, Control-click → Open no longer gets
past that. Before the first open, clear the flag:

```sh
xattr -dr com.apple.quarantine ~/Applications/Band.app
```

or open it once and choose System Settings → Privacy & Security → Open Anyway.
A quarantined copy's first open has failed with `Band: Invalid argument (os
error 22)` in `launch.log` (2026-11-01, macOS 26); after clearing the flag it
opened normally.

**Linux.** Unpack anywhere, for example `~/.local/opt`, then:

```sh
~/.local/opt/band-<version>-linux-<arch>/bin/band --install-desktop-entry
```

This adds Band to the app menu (`~/.local/share/applications/io.github.eelcoh.Band.desktop`).
When Qt lives in a Distrobox rather than on the host, add `--container NAME`; the
entry then runs `distrobox enter NAME -- …/bin/band`. `bin/band
--remove-desktop-entry` removes it. Only an entry Band wrote is ever replaced.
You can also run `bin/band` directly or link it into your PATH.

## Layout

Both platforms share one payload layout (on macOS it is
`Band.app/Contents/Resources/band`):

```text
bin/band-session        runtime, session service and launcher
bin/band-codex …        agent launchers (symlinks to band-session)
bin/band                Linux launcher script
libexec/band-ui         Linux Qt client (macOS: Contents/MacOS/band-ui)
integrations/           Pi extension and Copilot plugin used by the launchers
notices/                licenses for Band, libghostty-vt, fonts and themes
build-info.json         version and source revision shown in About
```

Shells started by Band get `bin/` on their `PATH` (`BAND_AGENT_BIN`), so
`band-codex`, `band-claude`, `band-copilot` and `band-pi` work inside Band
without extra setup.

## Where Band keeps things

| | Location |
| --- | --- |
| Settings and themes | `~/.config/band` (`XDG_CONFIG_HOME`, or `BAND_CONFIG_DIR`) |
| Sessions, window state, launch log | `~/.local/state/band` (`XDG_STATE_HOME`, or `BAND_STATE_DIR`) |

Settings from `~/.config/terminal-workbench` are copied once on first start.
State from the earlier preview (`~/.local/state/terminal-workbench/qt-preview`)
is adopted in place, so shells started by a preview keep running. Shells
started by an older install keep that install's helpers until they exit; keep
the old install until then.

## Not yet

Notarization and signing, an updater, an app icon, Flatpak or AppImage, and
bundling Qt on Linux.
