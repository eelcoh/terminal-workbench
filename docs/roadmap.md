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

Live Overview context now follows local foreground-process directories, Git
branches and linked worktrees through optional runtime metadata. Custom names
remain overrides, and old runtimes retain their labels. Linux automated coverage
passes; native Mac context acceptance remains pending. See [terminal context](terminal-context.md).

## Remaining work and next priorities

The core prototype workflows are implemented; the milestone table describes
acceptance gates rather than a list of wholly unstarted features. Remaining work:

1. Finish native platform/UX acceptance: Mac external-monitor text/borders and
   unplug recovery, keyboard layouts/IME, accessibility, Linux native Wayland and
   physical GPU behavior. The owner confirmed Mac resizing and horizontal content
   swipes; the mixed-display hardware review is pending.
2. Decide the production toolkit using those results and the approved minimal
   chrome/Overview flows. Complex text shaping and accessibility are still open.
3. Continue daily-use runtime hardening after creation/close journaling: shutdown
   validation of the explicit shutdown policy, broader crash/recovery and long-running sessions. Finish native terminal-search
   review, native folder browsing and invalid-path/recovery review.
4. Complete the core real-agent matrix: Linux Codex/Claude permission prompts,
   background-workspace behavior and trust/setup, plus Mac Copilot Working/Ready
   and full lifecycle transitions. Unsupported signals retain honest fallback.
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
  so shared settings can retain Command+Option on Mac and Ctrl+Shift on Linux.
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

## Keyboard polish: matching Linux and Mac modifiers

The owner prefers Super+Alt on Linux, matching Command+Option on a Mac keyboard.
Evaluate these as the common application/navigation defaults while preserving
saved custom bindings and platform clipboard conventions. Check compositor
shortcut conflicts, keyboard layouts, labels, help and migration before changing
defaults. This is a requested roadmap item.

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
Defaults are Ctrl+Shift+S on Linux and Command+Option+S on Mac; existing custom
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
GUI validation pass; native Mac review remains pending. See [session shutdown](session-shutdown.md).
