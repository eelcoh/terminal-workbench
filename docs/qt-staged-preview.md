# Staged Qt preview

`just toolkit-stage` builds the Qt candidate and copies a new, independent
preview into `.local/staged/qt-preview-…`. **It builds the preview without
opening a window.** The successful output gives its location and an executable
command under **Open a window**. Copy and run that command to start it.
No checkout, Cargo, CMake or `just` is needed to run the resulting preview.

This is a same-machine preview. The Python installation recorded at build time
and the Qt installation used to build it must remain available. The optional Mac Qt deployment below removes the installed Qt requirement for
Qt libraries/plugins once native loader review passes. Python remains external.
The production toolkit has not been selected.

## Mac: open the app

The printed directory contains **Terminal Workbench Preview.app**. Move that
app to a convenient location, such as `~/Applications`, **before its first
launch**, then open it in Finder. The app contains the runtime, service, agent
wrappers and client. The sibling `terminal-workbench` command works while the app remains beside it.
Moving the app alone keeps its payload intact; use the launcher inside the app
for terminal commands after that move.

The native bootstrap uses the recorded Python path, rather than searching
Finder's PATH. Python then replaces itself with the Qt viewer, retaining the
window lease. Locally built bundles are signed ad hoc; this is not a signed,
notarized distribution. Native Finder launch/signature acceptance is pending.
Bundle metadata follows [Apple's application bundle layout](https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/BundleTypes/BundleTypes.html).

To review: build, move the app, open it in Finder, type in a terminal, close the
window and reopen the app. The same shell should remain. `open -n "/path/to/Terminal Workbench Preview.app"`
opens another instance for the existing multiwindow flow.

## Mac: optional Qt deployment

On a native Mac, run:

```sh
just toolkit-bundle
```

This builds a separate preview with Qt frameworks, the Cocoa platform plugin
and QML modules deployed into the app. It uses `macdeployqt` from the Qt
installation selected by CMake, checks its version against the viewer build,
and scans a deployment manifest that includes the dynamically chosen Basic
Controls style. Deployment follows [Qt's Mac deployment tooling](https://doc.qt.io/qt-6/macos-deployment.html).
`just toolkit-stage` keeps its existing installed-Qt behavior on both platforms.
Linux bundling is not implemented. The owner confirmed the native Mac bundle
command completes at `164264b`; moved-app launch and loaded-library review
remain separate acceptance checks.

The script runs a static Mach-O linkage audit before the final ad hoc signature.
Unresolved or absolute external Qt imports fail the build. The audit reads
Mach-O load commands to distinguish actual imports from a library's own
`LC_ID_DYLIB` identification name. The latter is recorded separately as
`install_ids`; a Homebrew path there alone does not load an external library.
Actual imports remain checked, including weak/re-exported/upward/lazy imports.
See [Apple's distinction between IDs and imports](https://developer.apple.com/forums/thread/736719).
It also checks the
frameworks, platform plugin and required QML module metadata are present.
Workbench writes `Contents/Resources/qt.conf` itself after deployment and before
final signing, without relying on the Qt tool to create it. Plugin, QML and
library paths are relative to the app's `Contents`, so moving the app preserves
them. Any existing `qt6.conf` is aligned because Qt gives it priority. Copied
read-only files/symlinks are replaced within the stage, leaving their sources
untouched. See [Qt's configuration lookup and Mac path rules](https://doc.qt.io/qt-6/qt-conf.html).

Inside the resulting app, `Contents/Resources/workbench/` contains:

- `qt-dependencies.json`: Qt version, per-binary links, external non-Qt links,
  preserved notice locations and native review status.
- `qt-deploy.log`: output from `macdeployqt`.

The compiler's 100% marker is followed by staging, deployment, library checks
and signing. These phases announce themselves immediately. Deployment/signing
print elapsed-time updates every five seconds and the live log path; deployment
uses debug tool output in that log. Waiting tools also record their own process
and descendants (PID, parent, state, elapsed time and executable name) in the log;
unrelated processes and command-line arguments are excluded. The log starts with
the tool PID even when Qt itself writes nothing. Deployment is limited to fifteen
minutes and signing to two minutes. Each stage is fresh; deployment reuses
libraries already copied into that stage rather than forcing them to be
overwritten for each plugin. This avoids repeating library copies and link
rewrites. Timeout or Ctrl-C stops the packaging tool's process
group, including scanners/signing children, before removing the partial preview.
A failed deployment prints a log path outside that preview; signing has its own
`qt-sign-…log` alongside it. These logs survive removal of a failed stage.
This is an internal same-machine preview, not a standalone release:
recorded Python and any reported external non-Qt libraries remain required.
A static audit does not prove which libraries dyld actually loaded or that every
QML module can instantiate. Native acceptance is still pending.

Move the app before first use, close existing preview windows, and use the
activation steps below when replacing the service. Open the app in Finder;
check a terminal, Appearance, the folder picker, Overview and a second window.
Close and reopen it to check retained shells. To capture actual Qt loading,
run the moved app's bootstrap from a terminal:

```sh
DYLD_PRINT_LIBRARIES=1 QT_DEBUG_PLUGINS=1 \
  "/path/to/Terminal Workbench Preview.app/Contents/MacOS/workbench-preview"
```

Inspect the preview's `launch.log`: Qt frameworks and Qt plugins should load
from inside that app. Keep your installed Qt in place during this review.
Save that log with `qt-dependencies.json` when reporting the result.

The stage preserves Qt notice files found in the selected installation's known
license directories. If additional notices are supplied separately, after the
build run `python3 scripts/stage-toolkit.py --bundle-qt --qt-notices /path/to/notices`
to create a fresh stage preserving them too. Notice completeness and the chosen
Qt distribution's license obligations still require review before distribution;
`notices_review_required` remains true in the dependency report.

## Packaging size and timing

The native Mac bundle checks the selected `macdeployqt` tool's advertised
options. When supported, it passes `-no-codesign` to avoid that tool signing
binaries before Workbench adds its configuration, notices and dependency report.
The final Workbench ad hoc signing pass still runs after those files are ready.
It also excludes the `qmltooling` debugging plugin type when the Qt tool supports
that switch. Older tools keep their defaults; selected options are recorded.
Qt's [tool help and option handling](https://raw.githubusercontent.com/qt/qtbase/dev/src/tools/macdeployqt/macdeployqt/main.cpp)
define these switches. No Qt installation files are changed.

After a successful `just toolkit-bundle`, the output prints phase timings and a
size summary. A `bundle-metrics.json` file is placed beside the app in the
printed preview directory. It records copy/setup, Qt deployment, configuration,
linkage audit, notices and final signing times, plus logical regular-file bytes
and counts grouped into frameworks, plugins, QML and other files. Symlinks are
counted separately without following their targets. Compilation, archive size
and disk allocation are outside this report's scope. The total packaging time
can include small gaps between the named phases.

The metrics file is outside the app so writing it after signing preserves the
signature. Keep or copy that sibling file before moving the app alone. Internal
`qt-dependencies.json` also retains deployment/audit timings and selected flags.
For a Mac comparison, save the printed timings and metrics, then review Finder
launch, the folder picker, Appearance, Overview and another window on the moved
app. Report native speed/size changes from those results; portable fixture times
are not evidence of Mac packaging performance.

This slice removes the duplicate signing pass and debugging plugins. Other
Controls styles and nested Qt Quick modules still need a selective-deployment
investigation; they are not pruned by this change. The required Basic style and
dialog modules remain covered by the existing linkage/deployment checks.

## Linux: add to the app menu

On this workstation, build and run in `build-machine`. Move the **whole preview
directory** to its final location before first launch. From that location run:

```sh
./terminal-workbench --install-desktop-entry
```

This explicitly installs a user entry under `$XDG_DATA_HOME/applications`
(default `~/.local/share/applications`). Builds made in Distrobox record the
container name; the entry launches through `distrobox enter … --` on the host.
A native Linux build launches directly. App-menu registration does not happen
automatically while building. The entry uses the relocated absolute path, so
install it after moving the directory. Remove the entry
`terminal-workbench-qt-preview.desktop` to remove it from the app menu.
Existing entries without the Workbench preview marker are preserved.
The command uses [desktop entry Exec quoting](https://xdg.pages.freedesktop.org/xdg-specs/desktop-entry/latest/exec-variables.html), without shell command evaluation.

## Launch logs

Desktop/Finder starts write a private `launch.log` in the preview state directory,
by default `~/.local/state/terminal-workbench/qt-preview/launch.log`. Both launcher
and Qt output go there. A new start appends a timestamp; normal terminal launches
continue to show output in the terminal. If Python itself is missing, the Mac
bootstrap shows an error asking you to rebuild. Failures after Python starts
are logged; launcher errors also show an alert/notification when available.
A Qt loader failure can still exit with only details in the log.

## Shell startup on Mac

New default Mac shells start as login shells. A valid `SHELL` is honored; when
Finder does not supply one, Workbench reads the account's configured shell before
falling back to `/bin/sh`. Recognized shells use `-l` and the PTY supplies their
interactive terminal. With zsh this loads `.zprofile` before `.zshrc`, so the
package-manager/tool paths configured there are available to pyenv, atuin and
zoxide initialization. Workbench does not read or rewrite those files itself.
See [zsh's startup-file order](https://zsh.sourceforge.io/Doc/Release/Files.html).
Linux keeps its existing non-login default; explicit runtime commands keep their
existing behavior. The runtime additionally supports explicit login opt-in:
`workbench-session SOCKET --login-shell /bin/zsh`.

Rebuild and activate the new preview, then create a **new terminal** to review:

```zsh
[[ -o login ]] && echo 'login shell'
command -v pyenv atuin zoxide
```

Existing retained shells keep their startup environment. If an older service
cannot be activated, use the one-time service stop described below first. The
same new-terminal requirement applies after that manual service switch.

## User files and sessions

Run the terminal launcher with `--paths` to inspect paths without opening a
window, creating files or starting sessions. On Mac the launcher inside the
moved app is `Contents/Resources/workbench/terminal-workbench`.

- Preferences and themes use `~/.config/terminal-workbench` on both platforms.
  `XDG_CONFIG_HOME` and `WORKBENCH_CONFIG_DIR` keep their existing behavior.
- Preview sessions and window views use
  `~/.local/state/terminal-workbench/qt-preview`. `XDG_STATE_HOME` changes the
  base; `WORKBENCH_STATE_DIR` explicitly chooses a different session directory.
- New workspaces start in your home directory. Launching from a different
  current directory does not change the service's initial directory.
- Checkout sessions are not automatically imported. Existing stopped terminals
  stay stopped; closing a window detaches and leaves shells running.

Keep the preview in place while its sessions or service are running. Do not
replace its files to update it. Every staging command creates a fresh preview.
Opening another build against a running service still reports a configuration
mismatch until you explicitly activate that build. No update is automatic.

## Switch preview builds

From the new preview's final payload location, use its launcher:

```sh
./terminal-workbench --status
./terminal-workbench --activate
```

On Mac, that launcher is inside the new app at
`Contents/Resources/workbench/terminal-workbench`. Close all preview windows
before `--activate`; their shells may stay running. Activation reserves every
supported window slot, verifies the old service is a staged preview using the
same initial directory, and asks that exact service instance to retire. It
starts the selected service without creating a catalog, starting terminals or
replaying pending operations. Open the new app normally afterward. Repeating
activation of the selected service leaves its process unchanged.

Catalog identities, folders, window preferences, themes, running shells and
stopped entries remain. New terminals use the new build's runtime and helpers.
Retained shells keep their original helper paths, so **keep the old preview
payload until those shells have exited**. Activation does not update their PATH
or replace their runtime processes. A failed replacement startup leaves those
shells running; inspect `service.log` and retry a working build.

`--status` never starts a missing service or creates user directories. It reports
selected/runtime service paths, service health and whether activation is needed.
The JSON status includes `payload_usage`. Each observed build lists whether it
is selected, backs the current service, or supplies a running terminal's runtime
or helpers. After activating a newer build, retained terminals should still refer
to the older directory; newly created terminals should refer to the newer one.
Runtime origin is captured at startup, so a service switch does not relabel old
shells. Stopped terminals no longer add running references.

To inspect a moved Mac preview, run its installed launcher from a regular
terminal (adjust the app path):

```sh
"$HOME/Applications/Terminal Workbench Preview.app/Contents/Resources/workbench/terminal-workbench" --status
```

On Linux use `./terminal-workbench --status` from the preview directory. This
works with windows open. Under `payload_usage.builds`, `selected` identifies the
launcher's build and `service` the active organization service. A terminal ID
under `runtime_terminals` or `helper_terminals` still references that directory.
Use the accompanying service terminal list to map IDs to workspace/name.
New terminals from this build advertise origin; existing older terminals may
remain unknown until they exit and are explicitly replaced.

Older runtimes, unavailable endpoints and missing/non-staged paths are reported
under `unknown_terminals`. This report covers only the selected preview and the
current service's catalog. Other state directories, unlisted runtimes and
programs launched by a shell can have additional dependencies. **An absent
reference is not permission to delete a build.** The command never removes files,
starts shells or changes their ownership.

Older services without instance-checked retirement are refused. For a service
started by an earlier preview, close its windows and use the manual service stop
command below once, leaving shells running if desired; then activate the new
build. Do not use that legacy stop command while another update is in progress.

The activation command does not change the installed Linux desktop entry. Run
`--install-desktop-entry` from the newly activated payload to update the app-menu
link. Python/non-Qt dependency deployment, automatic updates and old-payload
cleanup are still pending.

To retire a preview: close its windows, first use **Stop all terminals…** if you
want to stop its shells, then stop the session service explicitly. Run from the
payload directory (the preview root on Linux, or
`Terminal Workbench Preview.app/Contents/Resources/workbench` on Mac):

```sh
python3 ./scripts/session_service.py stop \
  --state-dir "$HOME/.local/state/terminal-workbench/qt-preview" \
  --runtime "$PWD/libexec/workbench-session" --cwd "$HOME"
```

Use your actual state path if overridden. Stopping the service alone leaves
shells running. Removing the preview does not delete user preferences or state.

## Checks

`just toolkit-stage-test` checks launch after moving the payload, helper paths
with spaces, agent wrapper/plugin arguments, saved shell survival on viewer
exit, and stopped-terminal preservation. On Linux it starts the copied Qt viewer
with the offscreen platform in terminal and desktop modes, checking ready
renderers, exec handoff, configuration cleanup and retained window leases.

`just toolkit-desktop-launcher-test` checks a moved Mac app with its payload,
recorded-Python bootstrap arguments with a minimal PATH, private logs, exec
failure cleanup, released leases and retained shells. On Linux only the native
executable-location API is substituted; this is not a LaunchServices test.
Linux checks also validate the desktop entry and launch it through GLib with
spaces, Unicode and reserved characters, checking Distrobox routing and
preservation of unmanaged user entries. Native Mac Finder/signing and physical
Linux app-menu activation remain manual acceptance checks.


`just toolkit-activation-test` uses disposable staged builds with real PTYs to
check service handoff, stale-instance refusal, open-window exclusion and unchanged
catalog/preferences/view files, shell PIDs/variables and socket identities. It
also checks new-helper paths, retained stopped entries, startup failure/retry,
legacy-service refusal and isolation from unrelated service configurations.
Native Mac activation review remains pending.


`just login-shell-test` uses real zsh with a minimal PATH and private HOME/ZDOTDIR
fixtures. It checks profile-before-rc startup, lookup of tools added by the login
profile, explicit/default platform modes and same-PID reattachment without
repeating startup. `just check` includes this check. The Mac default is exercised
natively when running the recipe on Mac; Linux tests explicitly opt into login
mode while retaining their normal non-login default.


`just toolkit-bundle-test` uses portable fake deployment tools and Mach-O
fixtures to check version matching, QML import coverage, notice preservation,
link resolution/refusal, failure cleanup/log retention and audit-before-sign
ordering. It also checks ordinary staging and Linux refusal. These fixtures do
not execute Apple's loader, `macdeployqt` or native code signing.

`just toolkit-activation-test` additionally checks old/new payload references
across service handoff with retained and freshly created shells, and clears
running references after explicit stop. `just check` includes origin encoding
and legacy/unavailable/partial-reference reporting checks. Native Mac origin
reporting remains pending.
