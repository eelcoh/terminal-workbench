# Terminal details

Open a pane's menu → Terminal details, or find Terminal details in Commands.
Agent details also links to the same inspector. The target is the terminal's
stable identity: inspecting a background agent does not switch workspaces.

The dialog shows current attachment/ownership and the session service's latest
exit-status message. These are separate from the runtime report: an attached
shell can still be usable when its optional metadata is unavailable. Refresh
explicitly checks both sources again without starting a service or recovering,
reattaching, transferring or stopping a shell.

The runtime section uses one bounded read-only `CAPS` request per open/refresh.
It retains that report until refreshed or closed; it does not poll live health.
It shows the reported PID and whether snapshot reattachment, exit records and
extended agent metadata are advertised. Unknown capability names remain in the
expanded report. Not advertised means absent from this reply; it is not proof of
an installed CLI's behavior or a reason to stop an existing terminal. An older
runtime without CAPS, an absent socket, an unsafe endpoint, timeout or malformed
reply produces Unavailable and no automatic retry.

When the runtime provides origin metadata, Runtime launch path and Helper launch
path are the paths captured when that runtime started. The expanded report also
includes the viewer's configured runtime path. Equal paths do not prove equal
binary builds, and current filesystem contents do not replace launch-time facts.
Older or future origin formats remain unavailable independently of capabilities.
Runtime provenance comes from CAPS; the inspector does not scan processes,
inspect binaries or compare builds.

Retained terminals keep their original runtime and invocation-local `wb-*`
helpers across viewer rebuilds and preview activation. New agent support can
therefore need a newly created terminal. Create that terminal with the normal
New terminal action; existing work can keep running. This inspector performs no
upgrade or migration.

Copy diagnostics copies the same structured report shown under Show report.
Its explicit field set includes terminal identity, report state, capability
names, PID and local installation/launch paths. It excludes terminal text,
commands, workspace/project metadata, environment and arbitrary response fields.
It never copies a launcher command or writes terminal input. Copy is disabled
while a request is incomplete. Closing clears/cancels the inspection; a removed
target closes the dialog rather than inspecting a different pane. Neither
Overview nor Agents filters change.

## Validation

On Linux, run `just toolkit-runtime-info-test` for parser, endpoint and clipboard
coverage, `just toolkit-ui-test` for X11/Wayland native interactions, and
`just toolkit-qml-deployment-test` for the relocated QML closure. The checks use
private state and do not inspect or alter daily-use sessions.

For native review on either platform, build/open the current Qt workbench or
installed preview. Inspect a live terminal from Commands and its pane menu,
expand/copy the report, refresh and close with Escape. Its shell and ownership
should stay unchanged. Inspect an exited terminal: Stopped and its exit reason
should remain visible, with runtime details unavailable. Inspect a terminal
owned by another window: it should remain attached there. From a background
agent's Agent details, open Terminal details and verify the current workspace
stays selected. Use Copilot for Mac agent review; no extra agent install is
required. Native Mac review remains open.
