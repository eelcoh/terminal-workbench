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

## Build and install in one command

For the normal preview update workflow on Mac or Linux:

```sh
just toolkit-install
```

This builds the runtime/viewer and installs a new copy in a separate permanent
directory. No `SOURCE` argument is needed. On Mac it uses the owner-tested
selective Qt bundle path; Linux uses the same-machine stage with installed Qt.
Python remains external on both targets. The completed source stage is retained.

The combined recipe automatically runs the installed copy's `--check`. It saves
`installation-check.json`, raw output and a log beside the payload, outside the
signed Mac app. Successful verification prints exact activation/open commands
and the installation ID. Close preview windows and run `--activate`, then open
the installed app using the printed command. You can rerun `--check` if needed.
Building/installing does not activate a service or open a window. Keep older installs in place while retained runtimes or helpers
still reference them. `just toolkit-preview-list` shows observed references.

If copying fails after a successful build, the error identifies the retained
stage. Retry it with `just toolkit-install-preview SOURCE` after addressing the
copy error; no rebuild is needed. Existing stage/bundle recipes remain available
for reviewing build output separately, and the source installer below supports
copying an existing stage or choosing another installation parent.

A failed/timed-out check exits nonzero, retains the copied build, source stage
and diagnostics, and prints no activation/open instructions. Earlier installs and
sessions remain untouched. Check processes and their children are stopped on
failure/cancellation. The successful result records this check at that point in
time; it is not a fresh verification after external dependencies change.

The combined recipe has real Linux build/copy/check coverage under a private
install root, including copy- and verification-failure preservation. Mac command selection is
covered by portable tests; combined native Mac acceptance remains pending.

## Manage installed previews by ID

These checkout commands avoid copying long app paths. After `just toolkit-install`,
copy its printed **Installation ID**. `just toolkit-preview-list` lists those IDs,
observed service/runtime/helper references and the **last recorded** check result.
Older installs show `not-run`; corrupt or path-mismatched records are unavailable.
Listing stays read-only and does not run a new check.

Replace `ID` below with that installation ID or a unique prefix:

```sh
just toolkit-preview check ID
just toolkit-preview activate ID
just toolkit-preview open ID
```

Check reruns verification and updates the sidecar result without changing the
signed payload or session. Activate uses the same preflight, window exclusion,
handoff and restoration as the installed launcher's `--activate`; close preview
windows first. Open launches exactly that build (Finder on Mac, desktop logging
on Linux), without selecting its service. Each command performs only its named
action. Ambiguous prefixes, invalid installation records, escaping payloads,
linked installation directories and platform mismatches are refused before
execution. No implicit newest-build selection or old-build removal is added.

The `just` commands use the installer's normal root. For a custom root use
`python3 scripts/manage-toolkit-preview.py check ID --root /chosen/parent`
(substitute `activate` or `open` as needed). These are checkout conveniences;
the independent installed launcher and printed absolute commands remain usable
without the checkout.

## Install a preview alongside previous builds

After building a preview, use its printed stage directory as the source:

```sh
just toolkit-install-preview ".local/staged/qt-preview-…"
```

Replace `qt-preview-…` with the actual directory from the build output. On Mac
both the stage directory and its `Terminal Workbench Preview.app` are accepted.
This command copies an existing build; it does not rebuild, activate or open it.
Copy-only installation does not run the payload. To request verification too,
use `python3 scripts/install-toolkit-preview.py SOURCE --verify`.
Each invocation creates a separate permanent installation, even for the same
commit. Earlier installations and the source stage remain unchanged.

The default parent is `~/Applications/Terminal Workbench Previews` on Mac and
`$XDG_DATA_HOME/terminal-workbench/previews` on Linux (default
`~/.local/share/terminal-workbench/previews`). Each installation has its own
`preview-<revision>-<unique-id>` directory. The installer prints exact quoted
commands to check the installed copy, activate it after closing preview windows,
and open a window. On Linux it also prints the optional app-menu registration
command; builds made in Distrobox retain their recorded container launch path.

Use those printed commands rather than replacing an older app at a shared
location. Retained shells and helpers keep their original installation paths.
No old payload is removed and no service is selected during copying. The copy
is prepared in a private temporary directory and published only after it is
complete; a failed copy removes its partial directory. `installation.json`
lives outside the signed app/payload. Python and other external same-machine
dependencies remain unchanged. This is a preview installer, not release/update
management or automatic cleanup.

To choose a different installation parent:

```sh
python3 scripts/install-toolkit-preview.py SOURCE --root /chosen/parent
```

Real copied-launcher/check coverage passes on Linux after deleting the source
stage, with no session/configuration state created. Portable Mac fixtures verify
unchanged app file bytes and symlinks; the installed Mac copy still needs native
`--check`, activation and Finder launch review.

## List installed previews

Run from the checkout:

```sh
just toolkit-preview-list
```

The list shows each managed installation, whether the current session service
uses it, and the terminal IDs counted through runtime/helper origin references.
The regular display prints counts and paths; full terminal IDs and errors are
available as JSON:

```sh
python3 scripts/list-toolkit-previews.py --json
```

For installations made with a custom parent, or a different session namespace:

```sh
python3 scripts/list-toolkit-previews.py --root /chosen/parent --state-dir /chosen/state --json
```

Listing never starts a service, creates a missing installation/state directory,
opens terminals, changes the selected preview or removes files. An unavailable
service makes references unknown. Legacy/incomplete runtime origins and paths
outside the managed installation root remain explicit. Stopped terminals add no
running references. Unreadable installations are reported without hiding valid
siblings; installation-directory/record symlinks and escaping payload paths are
not followed.

These are observed references from one namespace, not a determination that an
older build is safe to remove. Child-program dependencies and other namespaces
are outside this inventory. The installer and listing tools use the same default
parent on each platform.

## Mac: open the app

The printed directory contains **Terminal Workbench Preview.app**. Prefer the
installer above for repeated builds. For a manual move, choose a new, unused
directory **before its first launch**, then open the app in Finder. Keep earlier
payloads in place while retained shells still reference them. The app contains the runtime, service, agent
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

## Check an installed preview

After building and moving the preview, run its own launcher with `--check`.
On Mac, for example:

```sh
"$HOME/Applications/Terminal Workbench Preview.app/Contents/Resources/workbench/terminal-workbench" --check
```

Use the actual app location if you chose another directory. On Linux run
`./terminal-workbench --check` from the relocated preview directory, inside the
same graphics/container environment used to run it. No rebuild is needed for
subsequent checks of that installed payload.

The command prints JSON and exits zero when its checks pass, or nonzero on
failure. You should see `"ok": true` at the top. It starts the runtime's usage
path and a short-lived Qt process, without creating terminals, acquiring a
window slot, connecting to the service or changing the selected preview. It is
safe to run while preview windows are open. The new check must be present in
the app you are testing; older staged copies do not gain it after a Git pull.

The Qt process creates Basic controls, layouts, and file/folder dialog
components without showing a window. Its report includes Qt version, platform,
QML/plugin search paths and loaded images. For Qt-bundled Mac apps, filesystem
QML imports are restricted to the app's copied QML tree, with Qt resource
imports retained. The check rejects observed Qt frameworks or known bundled
plugin names loaded from outside the app, including paths escaping through
symlinks. Mac also runs `codesign --verify --deep --strict`; it verifies the
existing signature and never signs or changes the app.

Mac image evidence comes from [dyld's loaded-image API](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dyld.3.html).
QML import reporting/restriction uses [QQmlEngine](https://doc.qt.io/qt-6/qqmlengine.html).
This is evidence for the modules loaded by this diagnostic process. It does
not open native dialogs, exercise GPU rendering or every app workflow, establish
notarization, or remove the external Python/non-Qt dependency requirement.
Retain the printed JSON with a report of any failure; it contains installation
paths and diagnostic errors, without reading terminal contents.

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

The owner measured Qt deployment at about 50 seconds after disabling redundant
signing, compared with a preceding 65-second run. These are individual native
observations, not repeated benchmark samples.

## Mac: selective QML deployment trial

The working default remains `just toolkit-bundle`. To compare a candidate that
copies scanner-selected QML modules instead of entire nested module trees, run:

```sh
just toolkit-bundle-selective
```

This builds a fresh preview without opening it. It uses Qt's generated
[QML import deployment script](https://doc.qt.io/qt-6.8/qt-deploy-qml-imports.html)
in a private temporary prefix, then moves the module metadata and plugins into
the staged app. The Basic style is explicitly included in the target's resource
scan because the app selects it dynamically. All scanner-declared dependencies,
including other Controls styles, are retained. No Qt installation files change.
`macdeployqt` then deploys and rewrites the libraries, followed by the same
configuration, linkage checks and final signing as the default bundle.

Save the sibling `bundle-metrics.json` before moving the app. Its
`qml_deployment.mode` should be `qt-cmake-import-closure`, with the selected
module list. Compare QML/framework/plugin sizes and the combined `qml_imports`
and `qt_deployment` timings with the default bundle's `qt_deployment` time.
An improvement on Mac has not yet been established.

Move the new app to its own location and launch it from Finder. Check a working
terminal, Appearance, Overview, the folder picker and another window. These
exercise both QML controls and native dialog integration; static dependency
checks alone do not establish loader or native UI behavior.

`just toolkit-qml-deployment-test` also exercises the actual Qt CMake API on
Mac or Linux. It creates Basic controls, layouts and dialog components using
only the copied filesystem QML import directory and Qt resource imports. It
checks that unrelated modules are omitted. On Linux it uses installed Qt
libraries with ELF RPATH adjustment disabled: this is a QML-copy proof, not a
standalone Linux package. Native Mac library deployment and opening dialogs
still require the bundle review above.

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

The isolated `just toolkit-activation-test` suite exercises candidate refusal,
normal handoff and post-retirement failure restoration with disposable service
namespaces and shells. It does not switch the installed user session. On Mac,
the suite signs its temporary fixture apps and re-signs deliberately modified
fault fixtures; native restoration acceptance remains pending.


From the new preview's final payload location, use its launcher:

```sh
./terminal-workbench --status
./terminal-workbench --activate
```

On Mac, that launcher is inside the new app at
`Contents/Resources/workbench/terminal-workbench`. Close all preview windows
before `--activate`; their shells may stay running. Activation first runs the
candidate's installed checks. Runtime/QML failures, and Mac signature or bundled
Qt loading failures, reject activation before creating session state or asking
the current service to retire. Run `--check` for full details of these checks.
Activation also rehearses the candidate session service in an empty, private
`/tmp` namespace: it checks health without creating terminals or a catalog,
then requests retirement of that exact temporary service instance. Startup has
a five-second deadline with short socket timeouts. Exit, timeout or interruption
cleans up the owned process group and temporary directory. A failed rehearsal
prints a private log path under `/tmp`; successful rehearsals remove their logs.
`--check` itself remains a runtime/QML check and does not start a session service.

Activation includes a brief Qt startup check, so use the same graphics
and container environment as the app.

After those checks, activation reserves every
supported window slot, verifies the old service is a staged preview using the
same initial directory, and asks that exact service instance to retire. It
starts the selected service without creating a catalog, starting terminals or
replaying pending operations. Open the new app normally afterward. Repeating
activation of the selected service leaves its process unchanged. Successful
activation reports a concise `verification` result alongside service status.

Catalog identities, folders, window preferences, themes, running shells and
stopped entries remain. New terminals use the new build's runtime and helpers.
Retained shells keep their original helper paths, so **keep the old preview
payload until those shells have exited**. Activation does not update their PATH
or replace their runtime processes. The startup checks do not guarantee that
starting the replacement service in the real namespace will succeed. If that
step fails after a previous service was retired, activation checks whether a
service is now responding. A responding or unverifiable service is left alone.
If none is responding, activation attempts to restart the previous preview's
service code with its recorded Python interpreter where available, otherwise
the current interpreter. It sends health only: no terminal recovery, catalog
initialization or mutation replay is performed.

Activation still exits nonzero for the failed update, and reports whether the
previous service was restored. If restored, reopen the previous preview's app
or launcher at the reported path. If restoration also fails, inspect
`service.log` and retry a known working build. The old payload and its external
Python dependency must remain available. This restores the session service, not
a previously dead shell process or a copy of saved files. A fresh install with
no previous service has nothing to restore.

A failed or interrupted real startup reaps only the newly spawned service
process group, so a timed-out startup cannot become an unexpected service later.
Existing independent terminal runtimes remain running. Once readiness succeeds,
a lost reply to a mutation is still reported without replaying it or stopping
the ready service.

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

Successful Linux activation now updates an existing Workbench-managed app-menu
entry to the selected payload, including its recorded Distrobox route. First-time
registration remains explicit: run the printed `--install-desktop-entry` command
once. An absent entry, unmanaged entry, symlink or non-default
session namespace is left alone. Repeating activation avoids rewriting an entry
that already points to this build. Remove the
`X-TerminalWorkbench-Preview=true` marker from a customized entry to opt out of
automatic following.

The activation JSON includes `desktop_entry`: `updated`, `already-current`,
`skipped` with a reason, or `warning` with an error. A launcher update failure is
a separate warning after successful service selection; it does not fail or undo
activation. Updates run under the existing activation/window reservation. Failed
candidate verification or service startup never retargets the app-menu entry.
Python/non-Qt dependency deployment, automatic updates and old-payload cleanup
are still pending.

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


## Identify the running preview

Open the application menu and choose **About Terminal Workbench**, or find the
action in Commands. The dialog shows the source revision and local-change marker
from the installed payload's `preview.json`. Details and Copy diagnostics include
the payload path, configured runtime, Python, actual Qt version and configuration
locations. Missing or invalid metadata is explicit in the details.

This identifies the current client. Retained terminals can use older runtime
helpers; use the preview inventory for those references. Diagnostics include local
paths, but exclude workspace names, terminal contents and environment variables.


## Hidden Workbench startup preflight

`--check` now loads the actual embedded Workbench QML as well as the generic Qt
control/dialog proof. It uses the ordinary client adapters and context setup
with an empty private catalog and temporary configuration. No window is shown,
window profile is leased, service is contacted or terminal is created. Managed
configuration, saved views and user themes are not loaded. Temporary check data
is removed on normal completion.

The `qml_startup.report` includes `client_ok` plus `client` workspace, terminal
and visible-window counts and QML errors. The overall check requires successful
queued Workbench initialization without QML warnings, with all three counts zero.
Mac bundled-Qt validation includes libraries observed during the real client
load. A generic-only result cannot pass the updated preflight. Activation uses
this check before service retirement.

This verifies construction of the empty client interface, including its static
dialog components. It does not exercise terminal delegates, open native dialogs,
render text on a physical display or replace interactive acceptance. Linux
checks cover relocated payloads, strict scanner-selected QML loading and rejection
after removing a required deployed module; native Mac review remains pending.


## Dependency evidence in the installed check

`--check` now includes a `dependencies` object. `python` identifies the running
external interpreter, resolved executable and version; `build_python` records
the interpreter/version used to stage a new preview. Older builds without that
record remain explicitly unknown. Python is still external.

`viewer_mappings` classifies startup paths against the whole Mac app (the payload
on Linux). Paths resolving inside the preview and conventional platform library
locations are counted separately; `outside_preview_paths` lists remaining paths.
An escaping symlink stays external. Unresolved/relative paths are unclassified;
missing or malformed loader evidence is unavailable, not an empty dependency set.
Mac evidence comes from dyld; Linux uses `/proc/self/maps`, which also includes
fonts and other non-library files.

`build_audit` separately exposes the static audit's external non-Qt link names
from `qt-dependencies.json`. Those names can include `@rpath` imports and are not
a list of libraries observed loading. Missing/invalid audits are explicit. This
informational inventory does not change the check's success policy or package
Python/libraries. It does not inspect Python's module dependency graph, deferred
libraries or programs launched inside shells, and makes no portability claim.

On Mac, run the printed installed-copy `--check` command after `just toolkit-install`
and retain its JSON output. Expect `dependencies.python.bundled` to be false,
`build_python.available` true for this build and `viewer_mappings.available` true
when viewer startup succeeds. External paths can be an empty list; do not infer
that the app is standalone. No service activation is needed for this report.


To review Linux app-menu following, first register an older installed preview's
entry with its printed `--install-desktop-entry` command. Build a new installed
copy with `just toolkit-install`, close Workbench windows, and run that copy's
printed `--activate` command. Expect `desktop_entry.result: updated`. Open Terminal
Workbench Preview from the app menu; About → Details should show the new copy's
`payload_root`. The saved shells should retain their PIDs. Repeat activation with
windows closed to check `already-current`; the entry should retain its timestamp.
Automated private fixtures are available through `just toolkit-activation-test`
and `just toolkit-desktop-launcher-test`.
