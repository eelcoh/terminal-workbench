# Roadmap

Terminal Workbench is an independent desktop terminal workspace for macOS and
Linux. The product combines quiet terminal-focused chrome with an expressive
workspace and agent overview. The visual and interaction quality should approach
[Dank Linux / DankMaterialShell](https://danklinux.com/): cohesive colors,
considered typography, fluid motion and responsive controls. Foot remains the
reference for simplicity. Both qualities are part of the target.

## Current position

The Linux integration prototype has established:

- Independent runtime-owned shell lifetime, detach and same-process reattachment
  through negotiated snapshots for new runtimes, with legacy replay fallback.
- Workspace/terminal creation, naming, stopping and saved organization.
- Terminal input, selection, clipboard, scrollback and independent resizing.
- Searchable spatial/list overviews, live previews, zoom transitions and saved
  drag-to-move/reorder without restarting sessions.
- Foreground agent identification and optional hook-driven Working, Needs input
  and Ready status. Real interactive CLI compatibility remains unverified.

The owner has confirmed a native macOS build/run. Full macOS input/interaction
acceptance remains open. Linux checks use X11/Xvfb; native Wayland and physical GPU
behavior remain unverified. The current renderer is an experiment; it is not the final toolkit.

## Milestones

| Order | Milestone | Completion evidence |
| --- | --- | --- |
| 1 | macOS integration proof | Native build and manual checks for terminal input, composition, selection, clipboard, resizing, detach/reattach and overview gestures. Record Linux native Wayland gaps separately. |
| 2 | UX spike and toolkit decision | An interactive design slice meeting the visual and interaction bar below, reviewed with the owner and demonstrated on both platforms before committing to a toolkit. |
| 3 | Ghostty-compatible theming | Theme selection, live preview/apply/cancel, saved preferences, coherent terminal/UI colors and dark/light support, without replacing running sessions. |
| 4 | Persistent runtime hardening | Runtime-owned terminal state, bounded snapshots/scrollback, detached query replies, a session service, and explicit exit/recovery behavior. |
| 5 | Complete workspace workflows | Smooth horizontal pane navigation, workspace directories, layout/focus persistence, terminal search and clear exited/disconnected states. |
| 6 | Fresh keyboard model | Scoped actions, configurable shortcuts, command palette and a way to forward intercepted bindings to terminals; platform input validation. |
| 7 | Real agent validation | Live Codex/Claude lifecycle transitions and hook trust/setup flows; explicit Unknown for unsupported agents or unavailable signals. |
| 8 | Installable releases | macOS app bundle and Linux installation, configuration outside the repository, lifecycle/update policy and platform smoke checks. Finalize the product name before distribution; the project uses MIT. |

The first UX slice now includes shared UI colors, rounded controls and
a searchable offline palette gallery with Night/Dawn, Dracula, Catppuccin and
custom-color previews, Apply/Cancel and saved preferences. Application appearance
is now separate from workspace themes and terminal overrides, with stable chrome
by default and optional follow-workspace colors. Working-view navigation now adds
numbered focus/visibility positions, header-scoped wheel/trackpad navigation and
continuous pane scrolling through intermediate terminals, with immediate input
and reduced motion. Overview scrolling now preserves fractional trackpad deltas
with bounded speed, short easing and separate horizontal gestures. Mac recovery, tap-to-click, new workspace/terminal creation
and continuous pane navigation have been confirmed by the owner. Full native
input and UX acceptance remain open. See
[appearance support](appearance.md). This starts milestones 2 and 3; it does not
complete the UX gate or choose the toolkit. UX exploration
and a small theme-loading proof can accompany platform validation. The UX spike
must inform toolkit selection; production theming follows the chosen integration.
Runtime hardening and platform/input validation are requirements before daily-use
readiness. Packaging follows these foundations.

Runtime hardening has started with runtime-owned VT state, bounded terminal-state
exports, detached query handling and negotiated snapshot-based reattachment. New
runtimes/windows reconnect beyond the legacy 16 MiB journal limit; old running
sessions retain replay fallback. Shared catalog changes now refresh across windows,
with explicit live-terminal ownership transfer and independent window focus.
An automatically reused organization service now routes window startup and catalog
commands, reports runtime health/capabilities, adopts surviving runtimes after service
restart, and requires explicit recovery for stopped saved terminals. Recovery UI,
creation/close journaling and installation lifecycle remain open. See
[the session service slice](session-service.md).
See [the hardening slice and measurements](runtime-hardening.md).

## UX spike: a product-quality gate

The UX spike is a dedicated milestone, not a final cosmetic pass. Build a runnable
slice covering the working terminal view, spatial overview, drag/reorder,
workspace creation and theme selection. Review the normal path as well as empty,
loading, disconnected, full-workspace and save-error states.

Evaluate typography, spacing, hierarchy, surfaces, focus, hover/pressed states,
status readability, interruptible motion, smooth scrolling, drag feedback and
small/large window behavior. Keep terminals prominent and controls discoverable.
Avoid solving visual polish by permanently adding more chrome.

Deliver a small visual language, shared UI tokens, interactive reference flows,
recorded platform results and a toolkit decision grounded in these flows. User
review establishes whether the result approaches the requested Dank Linux beauty
and feel. Passing functional tests alone does not satisfy this gate.
See [the UX spike brief](ux-spike.md).

## Theming direction

Use the [Ghostty theme format](https://ghostty.org/docs/features/theme) for terminal
color compatibility. Theme files use Ghostty configuration syntax and can contain
more than colors; the Workbench loader must define its supported appearance fields.
Using the Ghostty VT library does not establish theme-loading compatibility.

Planned scope:

- Load named presets and user-supplied Ghostty theme files without requiring a
  Ghostty installation. Pin any bundled catalog and preserve its license/notices.
- Support background, foreground, indexed palette, cursor and selection colors;
  define handling for missing values, malformed values and unsupported options.
- Derive matching Workbench surfaces, text, borders, focus and accent tokens;
  permit UI overrides where needed. Keep destructive actions and agent status
  distinguishable with text/icons as well as color, in dark and light themes.
- Provide a searchable theme picker, representative live preview, Apply/Cancel
  and persisted preferences. Apply to working terminals and overview previews
  without restarting processes or changing PTY geometry, input or selection.
- Support explicit light/dark choices and following system appearance on both
  platforms once the desktop integration is established.

Compatibility should be documented by supported fields and tested with real
Ghostty theme files. Full Ghostty configuration compatibility, wallpaper-derived
palettes and desktop-wide theme synchronization are later options, not
prerequisites for the initial theme milestone. Workspace themes and terminal
overrides are already part of the prototype.

## Release gates

- Native macOS validation and recorded Linux backend/input coverage.
- UX review against the agreed reference flows, with light/dark and reduced motion.
- No replay-cap failure or detached-query stall in supported long-running usage.
- Real-agent validation, honest fallback states and predictable keyboard routing.
- Safe persistence/recovery and installation outside the source checkout.

[Integration findings](integration-findings.md) records current evidence and limits.
[Architecture](architecture.md) and [keyboard requirements](keyboard.md) remain
implementation references; this document is the milestone sequence.
