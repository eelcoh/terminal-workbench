# Staged Qt preview

`just toolkit-stage` builds the Qt candidate and copies a new, independent
preview into `.local/staged/qt-preview-…`. It prints the launch command.
Move that whole directory to a convenient location **before its first launch**,
then run its `terminal-workbench` executable from a terminal. No checkout,
Cargo, CMake or `just` is needed to run it.

This is a same-machine preview: Python 3 and the Qt installation used to build
it must remain available. Build and run inside `build-machine` on this Linux
workstation. On Mac, build and run natively with the existing Qt/Python setup.
The copied Mac viewer bundle is an internal component; launch through
`terminal-workbench`, rather than opening that bundle directly. Finder launch,
bundled Qt/Python, signing, desktop integration and distributable packages are
still pending. The production toolkit has not been selected.

## User files and sessions

Run `./terminal-workbench --paths` to inspect paths without opening a window,
creating files or starting sessions.

- Preferences and themes use `~/.config/terminal-workbench` on both platforms.
  `XDG_CONFIG_HOME` and `WORKBENCH_CONFIG_DIR` keep their existing behavior.
- Preview sessions and window views use
  `~/.local/state/terminal-workbench/qt-preview`. `XDG_STATE_HOME` changes the
  base; `WORKBENCH_STATE_DIR` explicitly chooses a different session directory.
- New workspaces start in your home directory. Launching from a different
  current directory does not change the service's initial directory.
- Checkout sessions are not automatically imported. Existing stopped terminals
  stay stopped; closing a window detaches and leaves shells running.

Keep the preview directory in place while its sessions or service are running.
Do not replace its files to update it. Every staging command creates a fresh
preview directory. Live service handoff between staged versions, updates and
session migration are not implemented yet; opening another staged version
against the same running service reports a configuration mismatch.

To retire a preview: close its windows, first use **Stop all terminals…** if you
want to stop its shells, then stop the session service explicitly:

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
exit, and stopped-terminal preservation. On Linux it also launches the copied
Qt viewer with the offscreen platform and verifies that the window and terminal
renderers become ready. That is a startup check, not native display/GPU or Mac
packaging acceptance.
