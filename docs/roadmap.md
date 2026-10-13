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
  and Ready status. Real Linux Codex/Claude lifecycle validation has started; other agents and
  native macOS integration remain open.

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
| 6 | Fresh keyboard model | Scoped actions, configurable shortcuts, command palette, shortcut-help overlay and a way to forward intercepted bindings to terminals; platform input validation. |
| 7 | Real agent validation | Live Codex/Claude on Linux and GitHub Copilot CLI on macOS, lifecycle transitions and hook trust/setup flows; explicit Unknown for unsupported agents or unavailable signals. |
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
restart, and requires explicit recovery for stopped saved terminals. A graphical confirmation now starts a fresh shell only for a service-confirmed
stopped entry, with local unavailable-status feedback and live-runtime protection.
Detailed exit reasons, creation/close journaling and installation lifecycle remain open. See
[the session service slice](session-service.md).
Session-service startup, shared PID, stop and live adoption are also confirmed
on macOS; graphical stopped-terminal recovery is also owner-confirmed.
Per-window view persistence now saves size, independent terminal widths, stable workspace/terminal
focus and overview context in independently leased client slots. The owner has also confirmed native macOS
view restoration. See [window state](window-state.md).
Workspace project folders now support first/new/replacement shell startup,
shared folder editing and preservation of live shell directories during edits or
moves. Missing folders produce a local creation/recovery error. The path editor
is implemented; the owner confirmed macOS workspace creation, folder editing and
new-terminal directory behavior. Native folder browsing and remaining recovery/
invalid-path checks remain follow-up. Capacity and text-selection cues now explain
the eight-workspace ceiling and replacement of selected paths; the owner confirmed
these cues on Linux and macOS.
See [project folders](workspace-directories.md).
See [the hardening slice and measurements](runtime-hardening.md).

The fresh keyboard model now starts with a searchable Commands palette and a
client action registry. Mouse/chord entry, navigation, workspace/terminal dialogs,
capacity explanations and confirmed stops reuse established workflows. Shortcut settings now records and saves per-action bindings, rejects conflicts,
reserves shell control keys and includes terminal/workspace navigation defaults.
Send next key to terminal bypasses interception once, with cancellation and stable
terminal targeting. The owner confirmed smooth palette interaction; native Mac
acceptance of the new bindings and forwarding remains pending. See [Commands](commands.md) and [shortcut settings](shortcuts.md).

Real agent validation now exercises installed Linux Codex 0.161.0 and Claude
Code 2.1.296: foreground recognition, Ready/Working turns, interactive question
Needs input and same-process detach/reattach. Codex's normal hook-trust prompt
and untrusted Unknown fallback were observed. Permission-prompt behavior,
graphical background-workspace flows and native Mac integration remain open.
GitHub Copilot CLI is the owner's only permitted agent on the Mac and is a
required integration target. Native/npm identification and invocation-local hooks
are implemented. The owner confirmed native Mac identity in a new runtime;
The owner also confirmed the Mac Needs input filter tracks waiting agents;
Working/Ready and the complete native lifecycle sequence remain pending. See [Copilot testing](copilot.md).
Additional Linux agents can follow after this core matrix is reliable.
See [the live validation record](agent-validation.md).

The terminal font compatibility slice now renders Claude's Unicode logo and
status symbols using embedded primary/fallback glyphs; Linux screenshot coverage
passes. The owner confirmed glyphs on Linux and native macOS in Copilot.
Specific Retina scaling coverage, full shaping and broader font preferences
remain open. See [terminal fonts](terminal-fonts.md).

## Current UX slice: Niri-style terminal widths

The owner added Niri as an explicit reference for scrollable navigation, overview
polish and simple widgets. The prototype replaces the working view's window-wide
divider and fixed visible pair with independent terminal widths in each workspace's
scrolling strip: 1/2 → 2/3 → 1/3 cycling, snapped mouse resizing, three equal thirds,
mixed thirds/two-thirds and reversible focused-terminal full width. Widths follow
stable terminal identity rather than left/right positions and persist in each
window's workspace layout. Configurable actions use the existing Commands
and Shortcut settings surfaces. Review variable-width motion and native input on
both platforms before the toolkit/installable-build decision.
See [the detailed layout requirements](ux-spike.md#niri-reference-and-next-layout-slice).

## Current UX slice: shortcut-help overlay

The prototype now provides a Niri-inspired overlay showing current configured
bindings in themed groups. Editable defaults are **Command+Option+/** on macOS
and **Ctrl+Shift+/** on Linux, with Commands entry and an Edit shortcuts link.
Mac application defaults share Command+Option, except Command+C/V for clipboard.
Saved custom bindings remain intact. The owner has confirmed the independent
widths, their Mac navigation-consistent shortcuts and the help overlay.
Broader native acceptance remains open.

- Toggle the overlay with its shortcut; Escape and a visible close control dismiss
  it. Provide a **Show shortcuts** action in Commands for mouse access.
- Group actions by navigation, terminal/workspace management, terminal widths,
  overview/appearance and clipboard/input. Show the active configured bindings
  with platform-appropriate labels, including an explicit unbound state, rather
  than a hardcoded list of defaults.
- Offer a clear path to Shortcut settings. Keep the help action itself configurable
  and show binding edits when the overlay is reopened.
- Make help available from the working view and Overview, preserving the underlying
  view and focus when dismissed. Respect existing dialogs and shortcut recording;
  help input must not leak into terminals or trigger another action.
- Match application themes, typography and surface styling, with readable light/
  dark states, narrow-window scrolling, keyboard access and reduced-motion support.
  Opening help must preserve shell processes, PTY geometry and terminal state while
  background output continues parsing.
- Slash is supported by the shortcut parser/recorder. Validate slash versus
  question-mark input and modifier handling on actual Linux/Mac keyboard layouts, alongside conflicts, remapping and forwarding.

Acceptance includes custom and cleared bindings appearing correctly, predictable
open/close and modal routing, native keyboard checks on both platforms, and owner
review of the overlay's clarity and polish. See [shortcut help](shortcuts.md#shortcut-help-overlay).
This belongs to the UX spike and fresh keyboard milestone before installable releases.

## Keyboard focus slice

The prototype now supports Tab/Shift+Tab traversal and accent focus outlines in
management dialogs, Commands, help/settings/recording, Overview and Appearance.
Enter/Space activates focused buttons; text fields retain their quick Enter
actions. Stop/recovery requires an explicit move to the confirmation control.
Escape restores the underlying overlay/view, with Appearance cancellation
restoring the prior transaction. See [focus behavior](keyboard.md#dialog-and-overlay-focus).

Linux acceptance includes disposable-shell GUI checks, narrow help, light/dark
focus, and stop/recovery guards. The owner confirmed basic Mac focus navigation.
Broader modifier/non-US/IME behavior and accessibility review remain open before the UX gate is complete.

## Typography and surface polish slice

The prototype now separates a bundled sans-serif interface face from monospace
terminal content. Shared text roles improve captions, fields and dialog titles;
consistent corner radii and hover/pressed/disabled/action treatments refine
existing surfaces. Pane/navigation/help backing textures now render at display
resolution to address the owner's soft Mac terminal text report.
See [the typography slice](interface-typography.md).

Owner review of dark/light readability, native Mac text sharpness, Retina and
mixed-DPI monitors remains part of the UX gate. Complex shaping and native
accessibility remain toolkit requirements.

## Pane-header and sidebar refinement

The owner reports that the typography/sharpness pass is much better. The next
prototype slice adds quiet secondary controls, clearer selected workspace/pane
surfaces, workspace terminal counts and separate semantic agent status labels.
Long UTF-8 names use ellipses; pane geometry, action targets, terminal input and
keyboard bindings stay unchanged. See [the chrome refinement](interface-typography.md#pane-headers-and-workspace-sidebar).
Native Mac and owner visual review of this slice remain open.

## Minimal chrome design study

The owner's Niri/Dank Material/Foot references lead to a new
[minimal chrome study](minimal-chrome.md), with an
[interactive local sketch](prototypes/minimal-chrome.html). Top-bar indicators
represent panes: focus, visibility and off-screen position. A compact left rail
can expand into separate Workspaces and Agents views; occasional actions move to
application/pane menus. Compact and pinned navigation remain available for native owner review.
The owner approved the design. The native viewer now implements compact/pinned
navigation, cross-workspace agent attention, pane indicators and application/pane
menus with existing actions and confirmations. Platform visual/input acceptance
remains part of the UX gate; the HTML remains a sample-data reference.

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
