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
acceptance remains open. Linux checks cover X11/Xvfb and an opt-in native Wayland
viewer under nested Niri; physical GPU, input methods and multi-display Wayland
behavior remain unverified. The current renderer is an experiment; it is not the
final toolkit.

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
| 8 | Dotfiles and portable configuration | Documented readable settings outside the checkout, custom themes/fonts/shortcuts, platform overrides, predictable UI/file precedence and safe migration of prototype preferences. |
| 9 | Installable releases | macOS app bundle and Linux installation, configuration outside the repository, lifecycle/update policy and platform smoke checks. Finalize the product name before distribution; the project uses MIT. |
| Later | TUI apps / window-manager exploration | Named app launch profiles, an app launcher, workspace layouts and explicit live-instance/restart behavior, proven with real TUIs on Mac/Linux. |
| Later | Plugin exploration | After daily-use foundations and the first installable release: bounded extension points, capability/version contracts and explicit permission/lifecycle handling, proven with a small extension. |

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
New runtimes now persist confirmed exit/signal/stop reasons, with unknown fallback
for unrecorded losses. Creation and close now retain validated intent across interrupted operations;
installation lifecycle remains open. See
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
new-terminal directory behavior. Native folder browsing is now implemented and
the owner confirmed the Mac picker; remaining recovery/invalid-path checks are
follow-up. Capacity and text-selection cues now explain
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
and untrusted Unknown fallback were observed. Real permission-prompt accept/deny and graphical background-workspace flows
have now been reviewed with Claude 2.1.299 and Codex 0.162.1. Claude cancellation
can retain stale Needs input without a completion event; cancellation completeness
and native Mac full lifecycle integration remain open.
GitHub Copilot CLI is the owner's only permitted agent on the Mac and is a
required integration target. Native/npm identification and invocation-local hooks
are implemented. The owner confirmed native Mac identity in a new runtime;
The owner also confirmed the Mac Needs input filter tracks waiting agents;
The native Mac trace now confirms Working → Ready → Needs input → Working → Ready.
Normal exit, cancellation and other native edge cases remain follow-up. See [Copilot testing](copilot.md).
Additional Linux agents can follow after this core matrix is reliable.
See [the live validation record](agent-validation.md).

The terminal font compatibility slice now renders Claude's Unicode logo and
status symbols using embedded primary/fallback glyphs; Linux screenshot coverage
passes. The owner confirmed glyphs on Linux and native macOS in Copilot.
Specific Retina scaling coverage, full shaping and broader font preferences
remain open. See [terminal fonts](terminal-fonts.md).

Live Overview context now follows local foreground-process directories, Git
branches and linked worktrees through optional runtime metadata. Custom names
remain overrides, and old runtimes retain their labels. Linux automated coverage
passes; native Mac context acceptance remains pending. See [terminal context](terminal-context.md).

## Remaining work and next priorities

The core prototype workflows are implemented; the milestone table describes
acceptance gates rather than a list of wholly unstarted features. Remaining work:

1. Finish native platform/UX acceptance: Mac external-monitor text/borders and
   unplug recovery, remaining keyboard layouts/IME, accessibility, native Wayland
   edge cases and physical GPU behavior. The owner confirmed Mac resizing and
   horizontal content swipes; the mixed-display hardware review is pending.
2. Decide the production toolkit using those results and the approved minimal
   chrome/Overview flows. Complex text shaping and accessibility are still open.
3. Continue daily-use runtime hardening after creation/close journaling and
   confirmed session shutdown: sustained-session validation, snapshot responsiveness
   and service lifecycle. Native folder browsing and staged reconnection are now
   implemented; finish their Mac review, terminal-search review and invalid-path/recovery review.
4. Complete the core real-agent matrix after Linux Codex/Claude permission and
   background-workspace review: cancellation/event completeness, trust/setup
   edge cases, plus the remaining Mac Copilot exit/cancellation/permission and
   background/handoff acceptance after confirmed Working/Ready and waiting transitions.
   Unsupported signals retain honest fallback.
5. Extend portable dotfiles as needed, then package/install outside the checkout. Decide the
   product name, installed-app lifecycle and update approach before release.
6. Explore named TUI apps and Workbench as a TUI window manager, then plugins
   where extensions add something beyond launching terminal programs. Neither
   direction is a first-release gate. Finish native font-controls review and the
   menu/workspace-row polish below.

## Dotfiles and portable configuration

Treat application configuration as something the owner can keep in a dotfiles
repository and reuse on Linux/macOS. This milestone covers Workbench preferences;
management of shell dotfiles is a separate possible integration.

- Provide documented, readable configuration in native per-user locations, with
  an explicit path override and support for symlink-managed files. Migrate current
  checkout-local preferences without losing settings.
- Cover appearance, custom Ghostty themes, terminal/UI font preferences,
  shortcuts and workspace creation defaults. Support platform-specific overrides
  so shared settings can use matching Command+Option on Mac and Super+Alt on Linux,
  or preserve explicitly configured platform differences.
- Define defaults/file/platform/UI precedence and how UI changes are saved.
  Preserve user-owned comments and files or keep UI overrides in a separate file;
  avoid silently rewriting a user's managed configuration.
- Validate changes transactionally, report useful errors and keep the last valid
  settings. Define live reload versus restart-required settings explicitly.
- Keep configuration separate from session catalogs, sockets, agent credentials
  and per-window restoration state. Copying dotfiles must not copy runtime
  ownership or machine-specific session identities.

Acceptance: a shared dotfiles setup loads on both platforms, UI edits have a
predictable destination, malformed settings retain a usable app, and migration
preserves existing preferences and running terminals. The first slice documents its format and precedence; the remaining settings
and native acceptance still belong to this milestone.

The first dotfiles slice now implements shared/platform appearance and shortcuts,
native user configuration paths, read-only managed/symlinked settings, sparse
per-platform UI overrides and once-only preference migration. Scoped theme
identities stay with local sessions. Configuration reads occur at startup, with
no work added to pane rendering or runtime parsing. Font preferences, workspace
defaults, live reload and native Mac acceptance remain open. See [dotfiles](dotfiles.md).

## Apps: Workbench as a TUI window manager

The owner proposed apps as a longer-term product direction: use Workbench's
scrolling panes, workspace organization and Overview to manage TUI application
instances. A shell is one kind of pane; a Git interface, file manager, system
monitor or coding agent can be another. This builds on runtime-owned PTYs and
same-process reattachment rather than requiring a plugin system first.

Explore:

- Named app profiles in dotfiles: executable/arguments, working-directory rules,
  environment overrides, display name and preferred pane width. Support shared
  profiles with Mac/Linux overrides, without copying machine/session identities.
- An app launcher integrated with Commands and pane creation. Distinguish starting
  a new instance from focusing an existing one; launching a chosen profile is an
  explicit action.
- Project layouts that combine shells, agents and tools, with app identity in
  Overview and familiar pane navigation, resizing, workspace moves and ownership
  transfer across windows. Optional launch shortcuts reuse the fresh keyboard model.
- Separate saved launch recipes from live runtime instances. Closing a window
  detaches an app; reopening reattaches the same process while it is alive. After
  exit/reboot, show its stopped state and offer an explicit restart. Restoring a
  layout must not blindly rerun every saved command or imply restoration of the
  app's own internal state.
- Preserve each TUI's keyboard/mouse behavior, alternate screen, clipboard,
  scrollback and resize contracts. Keep Workbench navigation predictable through
  configurable chords and forwarding. Background apps retain the existing hidden
  presentation exits while their runtime continues parsing output.

Start with a small app-profile/launcher spike and real Git, file-management and
monitoring TUIs on both platforms. Acceptance includes focus versus new instance,
pane/layout restoration, long-running and exited apps, same-PID handoff, resize,
input ownership and missing-executable feedback. Launch metadata/process state
belongs to the runtime/session boundary; launcher layout and app indicators belong
to the client. No profile format, runtime API or app integration is selected yet.

This direction is on the roadmap, not implemented. Packaging and daily-use
foundations come first. Plugins can later add richer integrations where launch
profiles and ordinary terminal behavior are insufficient.

## Plugins: long horizon

Explore extension points after the first installable release and a stable
runtime/client boundary. Candidate uses include additional agent integrations,
Commands actions and project/workspace helpers. Start with a small extension to
prove a useful contract before building a marketplace or general UI framework.

Define versioned capabilities, explicit access to processes/files/network,
installation/removal and crash isolation. A failed or removed plugin must not
interrupt terminals or prevent startup. Keep core navigation, rendering and
session ownership under application control. Whether extensions run out of
process, and which APIs they receive, requires a separate design spike; no plugin
API or implementation is selected yet.

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
and **Super+Alt+/** on Linux, with Commands entry and an Edit shortcuts link.
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

## Keyboard polish: matching Linux and Mac modifiers

Linux application/navigation defaults now use Super+Alt, matching Command+Option
on Mac. Saved bindings and platform clipboard conventions remain intact; Commands,
help, menus and the recorder display the effective map. Defaults apply to omitted
actions; full-map files keep their saved keys. Niri/Zirconium's Super+Alt+S Orca
binding conflicts with search; Commands and remapping provide alternatives without
changing desktop configuration. The owner confirmed this change on 2026-10-14.
Linux GUI validation and remaining layout/compositor checks are tracked in
[shortcut settings](shortcuts.md#matching-application-modifiers).

## Chrome polish: menu sections and workspace removal

Restore the subtle separators between action groups in the hamburger menu,
as shown in the design prototype. Group terminal/workspace actions, settings,
clipboard and window actions for easier scanning while keeping the menu quiet.
Review spacing, keyboard focus, hover and narrow-window scrolling together;
separators must not become selectable rows.

Workspace removal currently takes too many steps. Add a quiet × at the end of
each workspace row, invoking the existing Stop workspace confirmation. Make
the workspace name and affected shells clear in the Are you sure dialog; only
confirmation removes the workspace. Keep row navigation distinct from the close
target, with keyboard access, a tooltip and visible hover/focus treatment. These
are implemented in the native prototype, with confirmation tied to the chosen
workspace's stable identity and no navigation on Cancel. Menu separators are
non-interactive; narrow-window scrolling and keyboard traversal retain actions.
Native Mac review remains pending.

Workspace name and project-folder fields now support caret placement, pointer/
keyboard selection, insertion/deletion and copy/cut/paste. Existing values open
with the caret at the end; Select all explicitly replaces the whole value.
Generated new names start selected. Long paths scroll with the caret and each
field retains its position through Tab. Linux checks cover Unicode, mouse/word
selection, clipboard limits, Save/Cancel, and unchanged live shell PID/cwd/grid.
Native Mac editing acceptance remains pending. See [project folders](workspace-directories.md).

## Overview polish and terminal context

The owner traced the empty Overview to reversed filter labels: All states was
shown while Needs input was active. The native prototype now uses explicit All
terminals / Agents choices and an always-labelled Needs input chip with active
styling. All terminals clears the status filter; empty filtered results offer
Clear filters and a filtered/total count. Theme is removed from Overview; use
the Workbench menu or Commands for appearance.

Overview now replaces generic shell names with live repository/branch/worktree
context, or the local working directory outside Git. Explicit custom names remain
overrides; directory and branch changes refresh through cached runtime metadata
outside rendering. Linux checks cover foreground-process cwd, linked worktrees,
detached HEAD, remote/unavailable fallback and stable shell/grid identity. Existing
runtimes retain their saved names. Native Mac acceptance and broader permissions/
remote integration remain open. See [terminal context](terminal-context.md).

## Design question: do terminal panes need names?

Reconsider the need for always-visible pane names and a Rename terminal action.
Compare a quieter header using the process/shell title, agent identity, current
directory or position, with custom names as an optional override. Evaluate the
working view, Overview, agent navigation and ownership/reconnect messages;
multiple similar shells should remain distinguishable. Stable terminal IDs and
session persistence remain independent of presentation labels. This is an open
design question, not a decision to remove names.

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

## Workspace-first Appearance study

The owner finds the four-target picker confusing and its text follow checkbox
less polished than the rest of the UI. The new [interactive study](prototypes/workspace-appearance.html)
explores one workspace theme picker, clear Apply/Cancel, workspace terminals and
chrome previewing together, and a steady picker surface. The scope tabs and
follow checkbox disappear from that flow. Terminal exceptions can remain in the
pane menu for remote-box colors; defaults remain in dotfiles.

The owner approved the study. The native picker now implements workspace-first
selection, fixed picker surfaces, clear Apply/Cancel and a separate terminal
exception entry in the pane menu. Existing palettes and persistence formats stay
intact. Workspace preview follows chrome automatically unless dotfiles explicitly
request fixed chrome; Cancel restores the prior follow state. Native Mac review
remains open. See [design requirements](workspace-appearance-study.md).

## Current UX slice: terminal font and size

The prototype provides font and terminal-text size controls for the current
window, plus configurable Increase, Decrease and Reset actions in Commands and
shortcut help. Interface sizing remains separate, so enlarging terminal text
does not enlarge chrome and dialogs.

- Provide a bounded, readable size range and show the current size. Select
  platform-consistent shortcuts after checking keyboard-layout and binding conflicts.
- Load the default from dotfiles, support platform overrides, and define how a
  per-window size adjustment is restored without rewriting managed configuration.
- Treat font size as logical points, independently of Retina/backing density.
  Rebuild primary/fallback atlases and presentation caches when required.
- Recompute terminal cells and PTY rows/columns, preserving shell PID, terminal
  state and scrollback. Genuine text zoom changes the grid; a density-only display
  move must still preserve its logical size.
- Validate input coordinates, selection, clipboard, pane widths, navigation,
  overview previews and live resize at small/large sizes, with one and fifteen
  terminals and native Mac/Linux review.

The native prototype now offers Terminal text settings with bundled JetBrains Mono
and Fira Code, live preview, Apply/Cancel, and 10–32 logical-point sizing. Commands
and configurable platform shortcuts provide Increase, Decrease and Reset. Dotfiles
set shared/platform defaults; each window restores its own differences. Interface
text keeps its independent font and size. Native Mac owner review remains open.
Separate UI scaling, arbitrary installed-font selection, programming ligatures and
per-terminal size overrides remain follow-up work. See [terminal fonts](terminal-fonts.md).

## Mixed-display and gesture slice

Mixed-density display moves now update font/canvas density without changing
logical terminal cells; the marked GLFW adapter fixes Mac scale callback
transforms and framebuffer refresh. Window recovery uses logical work areas and
pane outlines stay inside their clipping area. Horizontal two-finger content
swipes pan the working strip while vertical input retains terminal scrolling.
The owner confirmed native Mac resizing and horizontal swipes, and correct pane
borders on the LG 35WN75C at default 3440×1440. Text remained softer than native
Ghostty on that display. The owner confirmed the embedded Mac app/menu identity
fix. The CoreText rasterizer experiment gave no improvement and possibly worse
text; it has been reverted. Sharpness requires a controlled native comparison
before adopting further renderer changes. The earlier
external screen and unplug recovery remain pending; see
[display details](interface-typography.md#display-changes-and-pane-outlines).

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

## Theme imports from TerminalColors

The owner requested importing themes from [TerminalColors](https://terminalcolors.com/).
Explore browsing/searching its catalog or importing a chosen download/link into
Workbench's palette gallery. The site lists Ghostty among its supported formats;
reuse the existing Ghostty color-field loader where the downloaded format matches.

Provide an import preview with Apply/Cancel, a saved name/source and appropriate
author/license attribution, and an offline local copy after import. Establish the
available download/API contract and theme redistribution terms before integration.
Cover malformed/unsupported imports, duplicates and unavailable downloads without
changing existing themes or running terminals. This is planned; current import
support remains local Ghostty files. Font controls remain the active slice.

## Release gates

- Native macOS validation and recorded Linux backend/input coverage.
- UX review against the agreed reference flows, with light/dark and reduced motion.
- No replay-cap failure or detached-query stall in supported long-running usage.
- Real-agent validation, honest fallback states and predictable keyboard routing.
- Safe persistence/recovery and installation outside the source checkout.

[Integration findings](integration-findings.md) records current evidence and limits.
[Architecture](architecture.md) and [keyboard requirements](keyboard.md) remain
implementation references; this document is the milestone sequence.

## Current daily-use slice: terminal-output search

Find in terminal is implemented for the focused pane's retained output/scrollback,
with a compact query field, visible matches/counter, wrapping next/previous
navigation, Escape viewport restoration and Commands/settings/help integration.
Defaults are Super+Alt+S on Linux and Command+Option+S on Mac; existing custom
bindings keep precedence. Soft wraps, wide/combining Unicode and alternate-screen
isolation use the existing Ghostty state, with bounded incremental scans and
stale-result invalidation outside rendering. Continuous output can delay results
until a brief pause. Queries are literal/case-sensitive, capped at 255 bytes and
1024 retained matches. The Linux build/check and one/fifteen-terminal GUI cases
pass at ordinary and simulated doubled density; native Mac review remains open.
See [terminal search](terminal-search.md). Cross-terminal search, regex and case
folding are separate follow-ups, not part of this slice.

## Current runtime slice: stopped-shell reasons

Runtime-owned lifecycle evidence now distinguishes confirmed shell exit codes,
signal termination and explicit Workbench stops. The service/status CLI and
stopped-pane footer/recovery confirmation consume optional details. Service
restart preserves receipts, recovery clears prior-invocation evidence, and live
or uncertain endpoints remain authoritative. Unrecorded disappearance is not
labelled a confirmed crash/reboot. Old runtimes/status methods retain fallback.
Linux build/check cases pass. The owner confirmed the exit-code footer and
restart confirmation on native Mac. Application/session shutdown policy remains
a further hardening item.
See [terminal lifecycle](terminal-lifecycle.md).

## Current runtime slice: interrupted creation and close

Catalog creation now saves stable identities and pending intent before launching
shells. Reopening reconciles an interrupted operation, keeps surviving runtimes,
repairs projections and leaves missing launches stopped for explicit recovery.
Partial launch errors preserve already-started shells. Saved initialization never
silently starts replacement shells. Close intent is validated before stopping
anything; interrupted workspace closes finish without resurrecting removed entries.
Linux process-kill tests cover transaction boundaries, partial launch errors,
invalid intent, projection repair and sibling PID/state preservation. The owner
confirmed the automated catalog-interruption suite on native Mac; broader
graphical review remains open. See [catalog transactions](catalog-transactions.md).

## Current runtime slice: explicit session shutdown

Stop all terminals is available in Commands and the Workbench menu, with an
unbound configurable action in Shortcut settings/help. Confirmation captures
stable terminal IDs across workspaces/windows; terminals created afterward are
excluded. Cancel and unfocused Enter preserve shells. Confirmed shutdown keeps
workspace organization, names, directories, themes and window layouts; stopped
entries require explicit fresh-shell recovery. Window close continues to detach.
Interrupted shutdown intent is reconciled before subsequent mutations/recovery,
and old/unsafe endpoints fail before any initial stop. Linux backend and two-window
GUI validation pass; the owner also confirmed native Mac shutdown/restart.
See [session shutdown](session-shutdown.md).

## Current runtime slice: sustained-session validation

A disposable session soak now exercises continuous styled output, primary/alternate
screens, repeated snapshot decode/reattach and owner transfer, geometry changes,
detached cursor/size queries, orderly/forced organization-service loss and same-shell
state retention. A short case is required by `just check`; `just session-soak`
provides a longer one/fifteen-terminal workload with bounded diagnostics and
resource observations. This is backend/protocol validation; native window/rendering,
real-agent longevity and machine suspend/resume remain separate acceptance work.
See [session soak](session-soak.md).

## Current workspace/runtime slice: folder browsing and reconnection

Browse now opens the native platform folder chooser from New workspace and Change
project folder, keeping the selected path editable until explicit Save/Create.
Cancellation/errors preserve state and window close dismisses pending helpers.
Linux uses Zenity/the desktop dialog, with manual entry when unavailable; Mac uses
Standard Additions with separate path arguments. Real Linux picker and existing
editing/focus/ownership workflows pass; the owner confirmed the Mac folder picker
on 2026-10-14. Remaining edge-case acceptance is tracked separately.
See [folder browsing](workspace-directories.md#native-folder-browsing).

Snapshot reconnection now shows transfer/restore progress and bounded failure
details. Transfer work and history decoding yield between frames; a separate VT is
installed only after complete checksum validation. Existing input/ownership/legacy
fallback boundaries remain. One/fifteen-runtime slow-transfer GUI checks and real
Ghostty staging tests pass; synchronous prefix/page calls and native rendering
profiling remain limits. See [reconnection](runtime-hardening.md#reconnection-feedback-and-staged-decoding).
The owner confirmed the sustained-session soak on Mac.

## Current agent slice: permission/background review and lifecycle trace

Real Linux Claude 2.1.299 and Codex 0.162.1 now validate permission-driven Needs
input, background Agents counts/rows, filtered Overview return, approve-once
Working/Ready, completed agents leaving the waiting filter, and normal exit.
Claude denial can leave the last waiting state until a new lifecycle event;
Codex interruption clears it. These limits remain explicit.

`just agent-watch` now traces runtime-delivered identity/status transitions through
read-only subscriptions, without attaching terminals, changing sessions or
recording prompt/output content. It provides a simple native Mac Copilot follow-up
for fast Working/Ready transitions. Real-runtime/malformed-stream tests and a live
Codex trace pass. The owner confirmed native Mac Copilot tracing, including
Working/Ready and return from Needs input; the remaining native lifecycle edge
cases stay open.
See [agent review and tracing](agent-validation.md#linux-permission-and-background-workspace-review--2026-10-14).

The live review also exposed garbled/duplicated fast synthetic typing in interactive
agents. The ordered-input slice below fixes the reproduced callback-order and
packet-queue problems; broader physical keyboard acceptance remains open.


## Current input slice: ordered keyboard events

The rapid-agent-input finding now has a raw-PTY reproduction and a client fix.
An ordered GLFW callback bridge pairs each key with its own text/release/modifiers;
Ghostty's encoded bytes are batched to avoid overflowing the PTY packet queue.
Legacy/Kitty paused-window bursts preserve exact text and Enter order at 12 ms and
1 ms. Existing shortcut/dialog/clipboard behavior remains covered. The owner
confirmed normal native Mac typing on 2026-10-14; composition,
physical keyboard edge cases and IME acceptance stay open. See
[terminal input](terminal-input.md).

## Current platform slice: opt-in native Wayland

`just wayland-spike` builds a separate Wayland-only Linux viewer, sharing the
existing sessions and ownership model. The default X11 build and Cocoa build
remain available. Window placement follows the compositor. A private nested
Niri test validates text/Enter, Unicode bracketed paste, PTY resize, Overview
rendering and close/reopen without restarting the shell-side process.
The owner confirmed the Wayland desktop smoke check on 2026-10-15. Detailed
GPU performance, physical input/IME, selection/copy, trackpad and mixed-display
acceptance remain open. See [Wayland spike](wayland.md).


## Current input slice: non-US keyboard layouts

Native Wayland testing reproduced QWERTZ Ctrl-Z sending Ctrl-Y. The terminal
adapter now retains layout-specific unmodified characters and supplies suppressed
ASCII Ctrl text to Ghostty, preserving character callbacks for ordinary/composed
text and existing application shortcut positions. Private-seat checks cover
German text/Ctrl/Alt, Kitty press/release reporting, French AZERTY text/Ctrl-A,
and a US-international dead-key commit. Physical layouts, native Mac Option/dead
keys and full IME/accessibility acceptance remain open. See
[keyboard layouts](terminal-input.md#keyboard-layouts-and-composed-text).

## Current platform slice: Wayland display-scale transitions

`just wayland-display-gui-test` exercises real compositor scale changes through
1×, 1.25×, 1.5×, 2× and back in a private Niri session. Checks cover native backing
updates, captured pixel sizes, Overview cache refresh, stable logical window/grid,
exact input and same-process reattachment. The owner confirmed the suite passed
on 2026-10-15. This adds automated platform coverage;
physical mixed-display/unplug behavior, native Mac sharpness and GPU performance
remain open. See [display-scale validation](wayland.md#display-scale-transitions).


## Current platform slice: Wayland pointer coordinates

Native selection/copy checks exposed stale first-click coordinates on Wayland
entry and double-scaled mouse coordinates when opening at fractional density.
The platform patches now publish entry coordinates and retain logical mouse
scaling. Checks cover drag/reverse/Unicode clipboard round trips, SGR terminal
mouse input and Shift selection at 1×, dynamic 1.5× and fresh-window 1.5×,
with the same shell-side process. Physical trackpad and native Mac input review
remain separate. See [Wayland pointer validation](wayland.md#pointer-selection-and-clipboard).


## Current toolkit slice: input and semantic APIs

The [readiness audit](toolkit-readiness.md) identifies missing composition,
caret-aware IME positioning, accessibility and shaping integration in the
experimental viewer. An isolated Qt Quick probe now exercises preedit/cancel/
commit, caret geometry and accessible control roles/text/actions. Qt is the first
candidate to test, not an adopted frontend. Native IME, screen readers, Mac
execution and a real terminal-host proof remain acceptance gates. Run
`just toolkit-probe` for the window or `just toolkit-probe-test` for API checks.


## Current toolkit slice: Qt terminal host

An isolated single-terminal host now connects to the existing Rust runtime and
renders Ghostty cells through Qt QPainter with bundled Fira Code. Its disposable
Linux test covers snapshot/live output, keyboard and injected composition,
selection/clipboard, scrollback, PTY resize, owner rejection and same-PID reopen.
The main Workbench viewer is unchanged. Native Qt Wayland, real IME, terminal
accessibility, performance and Mac acceptance remain open; external Mac display
comparison is deferred until the owner has the screen again (about 2026-10-19).
See [toolkit readiness](toolkit-readiness.md#second-proof-an-existing-runtime-terminal-hosted-by-qt).


## Current toolkit slice: native Qt Wayland

The Qt host now has private native Wayland coverage for scale transitions,
backing pixels, stable logical geometry/grid, fresh pointer selection and Unicode
clipboard round trips, keyboard/composition injection, scrollback, exact PTY
resize and fractional-scale same-process reopen. The owner confirmed basic Mac
terminal-host launch/input. The main viewer is unchanged; physical display,
real IME, terminal accessibility and production performance gates remain open.
See [Qt Wayland validation](toolkit-readiness.md#native-qt-wayland-and-scale-validation).


## Current toolkit slice: accessible terminal text

The custom Qt terminal now exposes visible output text, cell-aware Unicode
geometry, caret/selection and focus semantics through an accessibility adapter.
Linux tests cover Qt interfaces plus real AT-SPI discovery, readable text,
selection/focus and live-output events using private buses/runtime paths.
Existing PTY input/clipboard/resize/reopen coverage remains part of the check.
Native Mac bridging, Orca/VoiceOver usability, sustained-output speech and history
navigation remain open. See [terminal accessibility proof](toolkit-readiness.md#terminal-text-accessibility-proof).


## Current toolkit slice: real IME service

The Qt probe now has a real IBus/Anthy keyboard-driven composition test on X11
and native Wayland. X11 isolates the toolkit/engine bridge; Wayland runs a native
window with `DISPLAY` removed. Both check cancel-without-input, conversion,
exact-once repeated commits and post-commit terminal keys over a private IBus
connection. Compositor-managed Wayland input-method protocols, candidate popup
placement and native Mac IME remain open. See [real IME validation](toolkit-readiness.md#real-ibusanthy-composition-on-x11-and-wayland).

## Current toolkit slice: composition presentation

The Qt terminal probe now honors IME segment formatting and composition cursor
position/visibility/color. The candidate anchor follows the composition caret.
Tests cover UTF-16 offsets and rendered styles; real-engine Left/Right movement
must not send terminal input. Native candidate popup placement and Mac IME
review remain open. See [composition presentation](toolkit-readiness.md#composition-formatting-and-caret).

## Current toolkit slice: composition focus

Real IBus/Anthy coverage now checks repeated terminal ↔ Copy-button pointer focus
changes and native window deactivation/reactivation on X11 and Wayland. Pending
composition cancels without PTY input, chrome typing stays outside the shell,
and return preserves the original attachment and fresh commits. No extra reset
policy was needed. The owner confirmed Japanese composition and the candidate
popup in the Mac Qt probe after enabling Japanese input. Native Mac cancellation,
exact-once commit and focus transitions, editable-terminal transfers and Linux
candidate panel placement remain open. See
[focus validation](toolkit-readiness.md#ime-focus-transitions).

## Current toolkit slice: shaping and fallback evidence

`just toolkit-text-test` generates a Mac/Linux comparison of actual Ghostty cell
drawing and Qt full-text shaping, with fallback-font/glyph reports and PNGs at
four raster scales. Linux evidence exposes Arabic, Indic, emoji and ligature
differences; long cell graphemes retain their text. The full-text reference does
not preserve terminal grid geometry by itself.

The owner supplied a passing Cocoa/Qt 6.12.0 run and comparison images; offline
Mac review reproduces the same main shaping differences with native fallback
fonts. Shaped terminal integration, physical Mac display acceptance and
one/fifteen-pane performance remain open. See
[shaping evidence](toolkit-readiness.md#text-shaping-and-fallback-evidence).

## Current toolkit slice: first grid shaping integration

The Qt terminal probe now shapes LTR style runs and maps their glyph groups
back to Ghostty-owned cells. Cached row images preserve cursor/selection geometry
and avoid shaping during paint or rerasterizing unchanged rows. The text proof
now includes integrated `grid-*.png` sheets alongside the old baseline. Tests
protect glyph retention, wide tails, emoji spans, style breaks, native operator
copy/paste and scale-specific row images. RTL keeps the previous per-cell policy.
The Mac integrated comparison validates on Cocoa/Qt 6.12.0: all seven LTR
font/glyph multisets match the reference, and joined operators/emoji are visible.
Live Mac interaction, spacing/clipping refinements and one/fifteen-pane
performance remain open. See [grid shaping](toolkit-readiness.md#first-grid-preserving-shaping-integration).

## Current toolkit slice: CPU raster/cache performance

`just toolkit-perf` adds Release 1/15-pane CPU measurements at fixed per-pane
80×24 geometry and 1x/2x raster scales, with three trials per ASCII/Unicode case.
The live host and benchmark share the visibility/cache boundary. Exact row-count
checks pass for retained/hidden/dirty/reveal scenarios; only one visible pane
stays near the single-pane raster cost despite 14 additional hidden sources.
Fully changing output and forced CPU compositing are costly at 15 full-size
panes, with roughly 121 MiB of row caches at 2x before composite buffers/fonts.
This is a CPU baseline, not the full performance acceptance gate. Mac results,
Qt Quick uploads/GPU/VRAM, live runtime load and physical-display behavior remain
open. See [performance baseline](toolkit-readiness.md#cpu-row-rastercache-performance-baseline).

## Current toolkit slice: reduce CPU glyph overhead

The Qt probe batches consecutive equal-font glyphs and uses a guarded bulk query
for plain ASCII. Other text retains the source-range mapping. Exact pixel and
source-span comparisons protect the previous output across scales and styles.
The repeated CPU matrix against source `7d5543f` shows 15-pane fully changing
ASCII frames drop from 88.3 to 42.3 ms at 1x and 118.9 to 74.1 ms at 2x, with
modest Unicode gains and no row-image memory reduction. Native Linux regressions
and `just check` pass. Native Mac optimization review, GPU/Qt Quick costs,
RTL/spacing and physical display acceptance remain separate gates. See [CPU optimization](toolkit-readiness.md#first-cpu-shaping-optimization).


## Current toolkit slice: retained image extent

The Qt foreground cache now omits unwritten trailing columns while preserving
full-grid backgrounds, cursor and selection coordinates. Exact full-canvas
pixel checks protect the reference output across scales and text/style cases.
The fifteen-pane 2x row caches shrink from 121.29 MiB to 112.19 MiB for ASCII
and 65.19 MiB for the shorter Unicode fixture; full rows save no space. Mac
benchmark output passes the operation-count checks, but its source revision is
unknown and native review of this new cache remains open. Linux native input,
Wayland/IME/accessibility and `just check` pass; forced retained copying improves
about 4% for ASCII and 16% for Unicode in the repeated CPU matrix. Future performance
logs identify the source checkout. See [retained image extent](toolkit-readiness.md#retained-row-extent-optimization).


## Current toolkit slice: live Qt Quick scene path

`just toolkit-scene-perf` adds native Mac/Linux painted-item measurements for
one/fifteen synthetic Ghostty terminals. It distinguishes cached scene redraws,
item repaints, dirty rows and fourteen hidden sources, with exact paint/reveal
checks, source/backend metadata, captures and process memory. The fixed viewport
scales fifteen full-resolution source surfaces into thumbnails. Request-to-swap
timing includes presentation pacing; GPU timestamps/VRAM and live PTY load remain
unmeasured. `toolkit-scene-perf-test` exercises the same path with software OpenGL
on private Linux Xvfb at 1x/2x. Native hardware/Mac acceptance remains open. See
[live Qt Quick measurement](toolkit-readiness.md#live-qt-quick-painted-item-measurement).


## Current toolkit slice: batch opaque backgrounds

Adjacent cells with the same opaque background now paint as row spans in the
Qt probe. Uniform 80×24 grids use 24 background fills instead of 1,920. Exact
pixel tests preserve color boundaries, fractional geometry and conservative
fallbacks for alpha/opacity/antialiasing/composition/transforms. In the repeated
software-OpenGL scene matrix, fifteen-pane unchanged repaint paint time falls
21–26% at 1x and 5–7% at 2x, with no row-cache memory change. All operation-count,
text, terminal, Wayland and `just check` regressions pass. Native Mac and hardware
measurements remain open. See [background spans](toolkit-readiness.md#opaque-background-span-optimization).


Native Mac scene evidence for clean `f65cc77` now passes all twelve trials on
Cocoa/Qt 6.12.0, Metal/Apple M2 Pro at 2x. Cached frames avoid painting, but
fifteen-pane unchanged repaints still spend 34–52 ms in CPU paint work; peak
process RSS reached 839 MiB across the complete run. These results support
paint/invalidation work while memory, exact native pixel equivalence, physical
screens and full live-runtime load remain open. See [Mac scene evidence](toolkit-readiness.md#native-mac-scene-benchmark-evidence).


## Current toolkit slice: repaint changed rows

The Qt probe now requests bounded repaint bands for changed rows and old/new
cursor bounds. Unchanged refreshes skip painting; resize, DPR, theme and active
composition preserve full invalidation. Retained-image pixel tests cover erases,
selection, cursor and coalesced updates at four scales. Scene trials compare
actual partial captures with forced full repaints and require exact equality.
In the 1/15-pane software-OpenGL matrix, fifteen-pane one-row paint time falls
about 95–97%; full-output workloads are not the target. Row-cache memory is
unchanged, and full-size image/texture/upload costs remain. Native Linux input,
Wayland/IME/accessibility and `just check` pass. The clean Mac upload now passes
all twelve partial/full scene comparisons and refreshed text checks. Mac
fifteen-pane one-row paint medians are 2.303 ms (ASCII) and 1.478 ms (Unicode).
Physical-display typography and live-runtime memory/VRAM acceptance remain open.
See [changed-row repaint](toolkit-readiness.md#changed-row-repaint-integration).


## Current toolkit slice: shared word navigation

The Qt terminal probe uses Option–Left/Right on Mac and Alt–Left/Right on Linux
for the same shell word navigation. Plain arrows, physical Control arrows and
additional modifiers retain terminal encoding. The portable real-PTY key test
checks exact bytes, held-key repeats and silent releases on both platforms.
The initial Mac-only implementation left Linux Alt arrows unbound at the shell;
this shared binding addresses that gap. Native Mac acceptance remains pending.


## Current toolkit slice: editable-terminal IME isolation

`just toolkit-terminal-ime-transfer-test` exercises two independently owned
editable Qt terminal windows with real IBus/Anthy on X11 and native Wayland.
Both pass pending-composition focus transfers, exact-once commits and key
isolation, stable reader processes and peer detach preservation. The original
refused-owner focus test remains available. Native Mac and two terminals in a
single window still need acceptance; live 1/15-runtime load, physical-display
quality and packaging remain open before choosing a production frontend.
See [editable transfers](toolkit-readiness.md#editable-terminal-ime-transfers).


## Current toolkit slice: same-window terminal input

`just toolkit-terminal-pair` hosts two disposable editable terminals in one Qt
window for Mac/Linux review. The Linux native IME proof now covers their shared
window/input context: composition cancels on pane focus changes, commits and
keys stay with the focused PTY, and toolbar Paste keeps B as its destination
when the button takes focus. Both xcb and Wayland pass with stable reader PIDs.
The original single-terminal and separate-window probes remain available.
Native Mac review, physical display quality, live one/fifteen-runtime load and
packaging remain open. See [same-window input](toolkit-readiness.md#same-window-terminal-composition-and-toolbar-routing).


## Current toolkit slice: live runtime performance

`just toolkit-live-perf` adds repeatable Mac/Linux CPU and memory measurements
for one/fifteen real PTY sessions feeding the Qt client. Idle, single-row,
full-screen, one-visible and all-hidden workloads check exact output bytes,
runtime/client final markers and reveal catch-up. The three-trial Linux
software-OpenGL matrix passes at 1x/2x. Hidden panes perform no raster or paint
work but retain row images; full-screen 15-pane 2x caches total 121.29 MiB.
Native Mac measurements and the approved UI design slice remain ahead; Qt is
still a candidate. See [live performance](toolkit-live-performance.md).

Mac pair copy/paste is owner-confirmed. Native composition focus policy stays
open; further manual Japanese testing is deferred. Preserve automated Linux
coverage and plan a language/input-engine matrix rather than treating one
Japanese engine as general language acceptance.


## Current toolkit slice: approved chrome around live terminals

`just toolkit-ui` now reproduces the minimal rail/top bar and pane indicators,
workspace drawer, independent width presets/full restore and spatial Overview
around six real disposable shells. Shared live items preserve PTY geometry and
processes through Overview; hidden/offscreen panes skip raster work. Keyboard
navigation uses Cmd–Option on Mac and Super–Alt on Linux, with shortcut help.
Native xcb and Wayland checks pass; Mac UI and motion review remain open.
Agents, creation/editing, themes and drag/reorder are subsequent slices, not
placeholder actions. See [UI study](toolkit-ui-study.md).

The owner supplied passing native Mac live-performance results from clean
d4b8ce4 (Qt 6.12/Metal, 2x): all six trials pass. Fifteen-pane full-output viewer
RSS is about 769 MiB, with median lifetime peak 877 MiB. Keep memory budgeting
and external-display quality open alongside UI review before toolkit adoption.


## Current toolkit slice: workspace and terminal creation

The owner confirmed the first Qt UI study works on Mac and Linux.
`just toolkit-ui` now uses a private instance of the existing session service
for live creation, with native editable workspace name/folder fields, inline
validation, busy/capacity states and cancellation. Cmd–Option–T/N on Mac and
Super–Alt–T/N on Linux create terminals/workspaces; menus and the top bar also
expose the actions. Stable IDs keep existing attached terminal items, widths,
focus memory and shell processes. Overview supports multiple rows per workspace.

Native xcb/Wayland creation checks cover current/empty workspaces, invalid
folders, cancellation, new-shell cwd and original-reader preservation. Launcher
checks include cleanup of shells created after startup on normal/failed exit.
Native Mac creation review remains open. Theme selection and drag/reorder are
next UI slices; no production frontend adoption is made here.
See [creation flow](toolkit-ui-study.md#creating-workspaces-and-terminals).


## Current toolkit slice: workspace Appearance

The owner confirmed Qt workspace/terminal creation on Mac and Linux.
The study now offers workspace-only Appearance through the application menu:
searchable offline catalog, All/Dark/Light filters, live terminal/chrome preview,
Apply and Cancel/Escape. The picker surface stays fixed through light/dark
previews. Each workspace keeps its choice for the current study window, and new
terminals inherit it. No extra theme button appears in Overview.

The existing Ghostty-compatible parser/catalog supplies all seven palettes.
Theme updates affect client defaults, cursor and selection, preserving explicit
RGB cells, program OSC overrides, shell identities and PTY geometry. Native xcb
and Wayland checks cover preview/cancel/apply, no results, workspace isolation
and inheritance. Mac Appearance review, saved configuration integration and
import remain open. Drag/reorder is the next UI slice.
See [workspace Appearance](toolkit-ui-study.md#workspace-appearance).


## Current Qt polish: cursor focus and shape

After owner confirmation of Appearance, the Qt host now renders a filled block
for the focused terminal in an active window and an outline for inactive
panes/windows. Application-requested bar/underline/hidden/hollow shapes are
preserved, with a cached shaped glyph under the block and a whole-grapheme caret
for wide text. Focus repaints the caret region without VT scans, row raster work
or a new idle timer. Native xcb/Wayland pixel and input checks pass; Mac cursor
review remains open. Drag/reorder remains the next larger UI slice.
See [cursor review](toolkit-ui-study.md#cursor-focus-and-application-shapes).
