# Architecture direction

## Ownership

The intended structure is a background session runtime and a graphical client.
Runtime-owned facts include workspace and terminal identity, child process
lifecycle, agent signals, and persisted session metadata. Client-owned state
includes selection, focused pane, overview visibility, window geometry, pane
arrangement, and shortcut bindings.

Persisting a workspace layout is distinct from preserving a live process. Closing
a window should leave the runtime alive; recovering after runtime exit requires
an explicit policy and cannot imply resurrection of arbitrary processes.

## Terminal and platform integration

Ghostty VT is the terminal foundation used by the experimental runtime. Ghostty
applications on macOS and Linux use a shared core with different native interfaces. Its full
embedding API and the VT-only library are different integration surfaces:
VT parsing alone does not provide fonts, rendering, selection, or native input.

Reference: <https://ghostty.org/docs/about>

Qt Quick is the production client toolkit on both platforms (decided
2026-10-22; see the [roadmap](roadmap.md)). The earlier Raylib/Ghostling viewer
was removed on 2026-10-23; sections below that mention it are history. The [readiness audit](toolkit-readiness.md) records the evidence
and the platform gates still to close.
The workspace layout remains experimental pending the remaining platform gates.

The current VT integration permits runtime-owned PTYs with clients attaching
to output and input. A replacement rendering surface must preserve this boundary.
If it owns child processes internally, establish the required adaptation first.
Avoid two unsynchronized authoritative terminal states, especially for agent
detection and resize handling.

## Herdr reuse

Investigate the public runtime API before extracting code. Check support for
terminal transport, input, resize, lifecycle events, and detection signals.
Herdr's TUI-oriented presentation transport may need adaptation; do not assume
it can directly drive an embedded graphical terminal.

Possible outcomes are using Herdr as a backend, extracting bounded components
with attribution, or implementing a focused runtime here. Record the decision
after examining the API and completing the terminal spike.

## Decisions still open

See [the roadmap](roadmap.md) for the current sequence. Theme loading
and matching application colors are implemented by the viewer, not VT parsing.

- Full renderer integration and version pinning.
- macOS application bundle and Linux packaging.
- Final product name. The project uses MIT; dependency notices remain separate.

## Spike outcome

A separate Rust-owned PTY and Ghostty VT viewer now work together on Linux and
macOS. Runtime ownership, attachment and reconnect have dedicated validation;
agent observations use runtime metadata and explicit integration signals.
See [integration findings](integration-findings.md) for the inspected API gaps,
measured behavior, and remaining platform/input checks. The Qt client
replaces this viewer as the production frontend.

## Workspace layout experiment

Launchers bootstrap independent runtime processes under stable socket
identities. An automatically started organization service (`band-session
service`, in Rust) owns saved workspace and terminal names, identities, startup and explicit
recovery. All launcher/management CLI requests reuse that authority. It preserves
`spike.sock` as Main's first shell
when migrating the old fixed catalog. The shared organization is stored in
`.local/sessions/workspaces.json`, which the Qt client reads.
New runtimes now retain their own VT state and support detached queries plus
text/binary state reads.
New viewers negotiate a pinned snapshot attachment on the control endpoint.
Capture and registration share the runtime event loop; the writer sends the
snapshot and ready boundary before queued live output. The client decodes into
a new VT and rebinds appearance/effects before feeding live bytes. Legacy
connections keep ordered replay and its 16 MiB limit. The initial session service
adopts live runtimes after its restart and retains
stopped entries until explicit recovery. Installation lifecycle, durable operation
journaling and a graphical recovery flow remain open.
See [session service](session-service.md).
See [the first runtime hardening slice](runtime-hardening.md).

Each viewer pane owns its transport queues, VT instance, encoders, selection,
render snapshot, and canvas. A UI-thread binding adapts the pinned example's
helpers to the current pane. Terminal geometry and pointer coordinates are local
to that pane; a separate render texture clips each terminal's presentation.
Selection, focus, divider ratio, and unread output marks are client-only facts.
These UI values reset when reopening the viewer. Saved organization remains
independent of their arrangement.

Hidden workspaces keep parsing their sessions and answering live VT queries, but
skip render snapshot updates and drawing. Their last terminal geometry is retained
until they become visible. No runtime protocol field or event was added for UI
layout. The output dot uses new live output received by an unfocused pane, excluding
historical replay; it does not classify an agent as working or blocked.

## Catalog management

The Rust service in `crates/runtime/src/service/` implements these rules for all
clients; it is the only implementation. Clients send neutral `workspace.create`,
`workspace.rename`, `terminal.create`, `terminal.rename`, `terminal.close`, and
`workspace.close` requests. The service serializes updates with an advisory
lock, validates identities and names, and atomically replaces the
JSON catalog. Unsupported or corrupt JSON
is reported and preserved instead of being replaced with defaults.

Renaming updates metadata without starting or replacing any session. Creation
starts only new sessions. Launcher initialization starts missing sessions, including
ones whose shells were ended. If a creation fails before metadata commit, only
the newly started, uncommitted runtimes are stopped. Existing sessions remain alive.

The C viewer reads complete projections before applying changes and reconciles
panes by stable identity. Pane resources live at fixed heap addresses, keeping VT
userdata, transport, and selections attached to surviving terminals when a sibling
is removed. New terminals receive separate encoders, selection, and queues. Each
workspace can hold up to eight terminals and shows at most two at a time;
Previous/Next brings a focused terminal into view. Workspaces are capped at eight.
Saved terminal moves and reordering are implemented. Workspace cwd settings and
a final runtime management protocol remain future work.

Explicit close stops runtime-owned shells through a separate private control
endpoint; it works while a viewer is attached. The runtime force-stops the PTY's
foreground process group and terminates/reaps its shell. Detached jobs are outside
this guarantee. Window close only disconnects views. UI confirmations require a
click on Stop or an explicit Tab/Shift+Tab move to its confirmation button before
Enter/Space. Opening Enter cannot accidentally authorize termination. Focus is
transient client presentation state and does not enter catalog/runtime protocols.

Before stopping, the helper records pending close identities in authoritative
JSON. Successful stops remove those identities and clear the intent. An interrupted
close is retried under the catalog lock before startup, so its shells cannot be
silently recreated. All old-runtime endpoints are checked before recording intent
or stopping any session; missing control endpoints produce a migration notice.
An operational failure may leave a partially stopped workspace with pending intent;
reopening retries the remaining stops. Empty workspaces and empty catalogs are valid.
The TSV projections described above were removed with the viewer
(2026-10-23); a legacy `workspaces.tsv` is still migrated when no JSON exists.

## Client overview

The terminal overview projects existing pane names, workspace names, connection
flags, and unseen-output marks. Search, list selection, scroll position, and the
open overlay are client-only. It enumerates up to 64 panes only while visible;
there is no process inspection, filesystem access, or terminal snapshot collection
in its row loops. Navigation changes client focus and reveals the target pane,
retaining its session and selection. Input scopes prevent search text and overlay
shortcuts from entering PTYs. Foreground agent identity now comes from a runtime-owned process sampler and
optional control subscription. It adds labels and a client-only Agents filter.
Working/waiting status comes from optional lifecycle hook reports, scoped to the
foreground group and invocation token. Connection/output marks do not infer it.
See [hook integration](agent-attention.md). See [identification](agent-identification.md).

Sampling occurs once per second per runtime, reading only the PTY group leader;
Linux uses one executable read and a bounded launcher read when needed. There are
no process-tree scans or filesystem reads in viewer rendering/overview row loops.
The sampler keeps one current identity; detached sampling does not enlarge replay.
Metadata fanout is bounded and independent of terminal attachment and Stop.


Spatial Overview presents filtered terminals as horizontal workspace rows. Its
selection, two-axis scroll offsets, view choice and preview caches are client
state. Opening it or resizing the window while it is open does not resize PTYs;
entering a terminal applies the ordinary working-view geometry. Never-opened
terminals use their existing runtime geometry and parsed viewport.

Preview textures are fixed at 384x240. Only visible cards allocate or update them;
unchanged previews reuse their texture, and dirty previews refresh at most 10 Hz.
Output revision, terminal geometry, viewport scroll offset and overview-open epoch
invalidate the cache. Offscreen terminals still parse output. Normal pane rendering
is skipped under the spatial overview. Preview camera scaling preserves aspect
ratio without changing screen rows, columns, selection or scroll position. Cached
textures are freed with their panes. See [spatial overview](spatial-overview.md).


The spatial transition uses a pure absolute-time easing timeline. Navigation is
pending client state until closing completes; toggling back discards that intent.
A cached full-window texture composites the overview while working canvases shrink
or the selected card enlarges. Preview textures refresh before drawing that stage,
so framebuffer scopes never nest. Only transition frames render the extra stage;
reduced motion avoids allocating it. The stage is resized on demand and freed on
window close. Terminal geometry/focus handoff occurs after closing, while PTY
parsing continues throughout. A bounded input lookahead preserves Raylib's queued
short presses and text when new typing completes a closing transition early.

Spatial dragging is client presentation state. The neutral `terminal.move` catalog
operation takes the stable terminal ID, destination workspace ID and final insertion
index. Validation precedes mutation; a cross-workspace destination must have room.
The helper saves JSON under the catalog lock, without starting or stopping a
runtime. The viewer changes placement only after helper success.

Catalog reconciliation searches every workspace for a terminal ID, preserving its
heap address and all live resources when ownership changes. Focus stays on a
surviving terminal in each workspace, falling back to the nearest source index.
The overview reveals the moved terminal after saving while ordinary PTY layout
remains deferred until leaving Spatial Overview. Drag ghost rendering reuses a
preview texture and performs no runtime work or additional texture allocation.

## Client view persistence

The launcher leases one reusable window slot, separate from the organization
service. Its viewer saves size, per-terminal width presets/full-width state, normalized
working-view positions, stable identity selections and
overview context in a private versioned client file. Closing releases the slot
and preserves live terminals. Restoring a view never claims another window's
terminal or recovers a stopped process. See [window state](window-state.md).

## Workspace directories

An optional canonical project directory belongs to shared workspace organization.
It controls the cwd of newly started and recovered shells; live process cwd stays
with each runtime. Folder editing only updates metadata. The viewer projects the
shared setting for editing; validation and startup happen in the organization
service. See [workspace project folders](workspace-directories.md).
