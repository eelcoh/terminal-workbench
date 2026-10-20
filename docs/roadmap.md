# Roadmap

Terminal Workbench is an independent desktop terminal workspace for macOS and
Linux. The product combines quiet terminal-focused chrome with an expressive
workspace and agent overview. The visual and interaction quality should approach
[Dank Linux / DankMaterialShell](https://danklinux.com/): cohesive colors,
considered typography, fluid motion and responsive controls. Foot remains the
reference for simplicity. Both qualities are part of the target.

## Current position

As of 2026-10-19, `just spike` remains the daily-use viewer. `just toolkit-ui`
is a separate, disposable Qt candidate with the approved minimal chrome, live
Overview, creation/folder editing, workspace Appearance, keyboard and pointer
pane moves, a drop-in Ghostty/TerminalColors theme directory,
and live context/agent filters.
Imports are explicitly saved as an offline palette collection; study sessions
remain private. Native Mac acceptance of the latest moves/imports/agent Overview
is pending. No production frontend switch has been made.

`just toolkit-workbench` now opens the Qt candidate on the existing saved
catalog without study seeding. Closing detaches instead of stopping shells.
Workspace organization and live processes persist. Qt layout/focus/Overview and
workspace colors now restore from separate leased view profiles. Cross-window
catalog updates now arrive automatically. The owner confirmed appearance retention
with `f80f00b`; broader Mac view/display acceptance remains open. See
[the saved-session slice](qt-saved-sessions.md).

The Qt candidate now follows the approved `just spike` chrome: bundled interface
font, compact top bar/rail, geometric icons, pane-position pill/count and themed
controls. The owner approved its revised appearance. The spike remains the
visual reference. Overview previews use
the normal arrow, with a grab cursor only during dragging.

The Qt candidate now includes terminal lifecycle/ownership controls: recorded
exit reasons, non-stealing reconnect, guarded recovery, explicit ownership
transfer and confirmed terminal close, now owner-confirmed. The command palette
adds searchable, context-aware actions with shared creation/layout dispatch and
safe dialog handoff. Native Mac palette review remains open. Next are the
remaining workspace workflow parity and packaging. Supported Qt application chords now
load from managed dotfiles and sparse platform UI overrides, with active
Commands/menu/help hints. The recorder/editor now supports Apply/Cancel, clear
and managed-default restoration; one-shot key forwarding is implemented; native Mac recorder/forwarding review
remains open.
The Qt terminal-text slice now loads managed shared/platform font defaults and
provides bundled face/size preview, Apply/Cancel and size shortcuts. Apply now saves font defaults in a sparse platform UI file; size shortcuts
remain temporary until explicitly applied. Saved window layouts and workspace themes now use leased view profiles. The owner confirmed Mac font
selection and sizing; the unrelated theme-file fallback warning fix is pushed
and awaits a Mac recheck. Retain the external-display/accessibility/native-input
acceptance gates before adoption. The sections below record evidence and remaining scope.

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
5. The Qt candidate now has a same-machine staged preview outside the checkout:
   `just toolkit-stage` creates a movable launcher/payload with separate user state.
   Finder bundles and explicit Linux app-menu registration are implemented, with
   private desktop launch logs. Ordinary staging uses installed Qt/Python.
   Finish native Finder/signature and app-menu review. Explicit `--status`/`--activate` now support
   service handoff after closing windows, retaining existing shells and saved state.
   Status now reports runtime/helper build references for retained and new
   terminals; legacy or unavailable origins remain explicit. This is an observed
   inventory, not automatic cleanup or a deletion-safety assessment.
   Mac default shells now use login startup/account-shell fallback so Finder
   launches load profile-provided tools. Optional `just toolkit-bundle` now
   deploys Qt on native Mac builds with static linkage reporting and preserved
   notices. The owner confirmed Mac bundling at `164264b`. Bundling now measures
   phases/size and uses supported Qt switches to avoid duplicate signing and
   debugging plugins. The owner measured Qt deployment at about 50 seconds versus
   a preceding 65-second run. Opt-in `just toolkit-bundle-selective` now uses
   Qt's CMake import closure; a real Linux copied-QML load check passes. The owner
   confirmed the trial works on Mac at `5a6c9f4`; native size/time comparison
   remains open. Installed `--check` now verifies runtime/QML startup without
   sessions, with observed Qt containment and existing-signature verification
   on Mac; the owner supplied a passing Mac verification report at `5d03edc`.
   Activation runs those checks before service retirement; Linux failure-isolation
   and retained-shell handoff checks pass. Activation also
   rehearses candidate service startup in a private zero-terminal namespace,
   catching daemon startup failures before retirement; Linux cleanup and
   retained-service checks pass. The owner confirmed a Mac activation cycle at
   `5d03edc`: new/restarted terminals remained running while older stopped
   entries were left untouched; the earlier runtime loss is not explained.
   Real service startup now also bounds readiness
   and reaps its own failed/interrupted child group, preserving the no-replay
   boundary for sent mutations; Linux lifecycle and activation tests pass.
   Pane-chrome focus is confirmed
   on Mac at `f820f0f`. Native shell startup,
   activation and moved-bundle loader/launch review, Python/non-Qt dependencies
   and the remaining installed lifecycle still
   precede an installable release. Decide the product name and update approach first.
   See [the staged preview](qt-staged-preview.md).
6. Explore named TUI apps and Workbench as a TUI window manager, then plugins
   where extensions add something beyond launching terminal programs. Neither
   direction is a first-release gate. Finish native font-controls review and the
   menu/workspace-row polish below.

## Qt navigation: pinnable left panel

The Qt panel now supports pin/unpin with separate Workspaces and Agents tabs.
The compact rail remains the default. Pinning reserves a column beside the
terminal area; navigation and application shortcuts stay available. Unpinning
returns to the temporary drawer. Below 920 pixels the column collapses into the
drawer while retaining its pin preference for a wider window.

The dedicated agent list shows identity, lifecycle status, workspace and terminal
context across all workspaces. Selecting a row focuses that terminal directly,
leaving Overview if necessary. It neither changes Overview filters nor transfers
terminal ownership. Existing read-only observers supply the facts.

Saved Qt view profiles retain the pin preference, with older profiles defaulting
to compact navigation. Native Linux checks cover pinning, workspace navigation,
narrow fallback, restart restoration and cross-workspace agent focus. Native Mac
review remains required. Overview refinement is tracked separately below.

## Qt Overview visual refinement

The first refinement now adds clear workspace headings/folders/counts, quiet group
separators, native-size context/status captions and unscaled selection/hover
outlines. Narrow windows use fewer columns and taller previews. Keyboard navigation
reveals the selected preview and caption; the same geometry drives drag targets.
Filters show their scope/count and offer Clear filters after an empty result.
Empty workspace backgrounds remain plain. Dedicated left-panel agent navigation
stays separate from spatial Overview. Native Mac visual/trackpad review and owner
acceptance remain open. See [Overview refinement](toolkit-ui-study.md#overview-refinement).

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

Qt now also provides a separate × on each row, with a stable-ID confirmation and
Cancel initially focused. Menu/Commands removal uses the same existing service
operation. Peer changes cannot retarget the confirmation, and removing the final
workspace leaves a usable empty window. Creation is a distinct fixed-footer action
below a divider with a plus icon; workspace arrow navigation stays among existing
rows. Native Mac acceptance remains pending. See
[Qt workspace removal](toolkit-ui-study.md#workspace-removal-and-the-separate-create-action).

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

## Qt external-display comparison diagnostics

The LG screenshot supplied on 2026-10-19 shows intact Qt pane borders, but different
font sizes prevent a controlled sharpness comparison with native Ghostty.
`just toolkit-display-report` now launches saved Qt sessions with opt-in metadata
for the resolved base font, cell metrics, window/display DPR, retained row-image
DPR, device-pixel origins and graphics backend. Reports update after relevant
client changes, outside render/parse, without terminal text or project paths.
Native Linux checks cover startup, font/resize updates and content-free output.
This adds evidence for the Mac display acceptance gate; it does not change the
rasterizer or claim that sharpness or unplug recovery is fixed. See
[the matched comparison procedure](toolkit-ui-study.md#external-display-font-comparison).

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

## Ghostty and TerminalColors theme directory

The primary workflow is a user-managed `themes/` directory under the Workbench
configuration directory. Drop Ghostty files there and select them in Appearance;
no import wizard, pasted URL or extension renaming should be required.
TerminalColors downloads ending in `.default` must work, alongside `.theme`,
`.conf` and extensionless Ghostty themes. Open Appearance again to discover new
or changed files. Keep installed choices offline and never rewrite user files.

Retain workspace preview/Apply/Cancel and clear invalid-file feedback without
losing valid themes or restarting terminals. Explain automatic discovery beside
an Open themes folder link, rather than a button suggesting a file selector.
Remove the import form while preserving previously saved imported themes. A remote catalogue browser is deferred; it is not required for this
simple local workflow. Discovery, edits, cancellation and malformed-file isolation
are implemented and covered on Linux X11/Wayland; Mac folder opening/discovery
remains for owner acceptance. See [the directory workflow](theme-directory.md).

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

## Previous TerminalColors imports

The Qt candidate retains previously imported themes and their saved offline
metadata; the import form has been removed.
The owner prefers a simpler primary flow: drop Ghostty files into the user themes
directory, including unchanged `.default` downloads. Directory discovery is now
implemented; see [the drop-in theme workflow](theme-directory.md).

Remote catalogue browsing is deferred in favor of the directory workflow. Native Mac directory opening/discovery remains an acceptance item.

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

## Qt parity: find in terminal

The Qt candidate now reuses the prototype's bounded literal UTF-8 scanner, with a
lazy GUI-thread controller for debounce/batches, tracked viewport restoration and
cached highlight spans. A compact native editable bar supports match navigation,
Commands/menu handoff and the existing configurable search chord. Only the selected
terminal scans; output/reflow invalidates results, closed panes do no search work,
and queries do not enter PTY input or saved views. Retained stopped text can be
searched without reconnect/recovery. Controller/shortcut and native Linux UI
checks cover the slice; native Mac visual/input acceptance remains open. See
[Qt terminal search](terminal-search.md#qt-workbench).

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

## Qt parity: explicit session shutdown

Qt now exposes the existing `session.stop` through a distinct Stop all terminals
confirmation in Commands/the menu. Frozen saved IDs cover workspaces/windows;
Cancel/initial Enter preserve shells, new terminals are excluded, removed targets
disable submission, and organization/appearance remain. Closing still detaches.
Filesystem-driven, read-only lifecycle invalidation updates disconnected peers
without polling, reconnect or recovery, including invalidations racing an old
status reply. Native Linux model/two-window checks exercise the slice; Mac Qt
acceptance remains open. See [Qt session shutdown](session-shutdown.md#qt-workbench).

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


## Qt study: Overview drag and reorder

The owner confirmed cursor styling works. Plain clicks in the terminal now clear
selection without selecting the clicked cell; deliberate text drags retain
selection/copy behavior. Overview previews can be dragged to reorder or move
between workspaces, including empty ones. A destination outline/insertion marker,
Escape cancellation and edge scrolling guide the move. Full destinations reject
moves while allowing same-workspace reorder.

The UI submits the existing `terminal.move` service command, then projects the
confirmed catalog onto stable terminal items. Width preferences, live shells,
cwd and attachments survive; themes follow the destination workspace. Keyboard
navigation and focus memory follow the confirmed order. Native xcb/Wayland checks
cover drag, cancel, empty destinations, edge scrolling, theme inheritance,
reordered navigation and unchanged grids, raw-reader PIDs and input isolation.
Native Mac click/drag review and keyboard-only move commands remain open.
This is still the disposable Qt study, not a production frontend switch.
See [moving terminals](toolkit-ui-study.md#moving-terminals-in-overview).


## Qt study: workspace name and folder editing

The working-view application menu now exposes Rename workspace and Workspace
folder as separate Save/Cancel forms, reusing the creation fields. Existing
values retain their text with the caret at the end for in-place edits. Empty
values disable Save; invalid folder errors stay inline. The UI submits existing
`workspace.rename`/`workspace.directory` methods against the private study
service. Each form saves one setting without a combined partial-success path.

Confirmed catalogs update the workspace labels and the folder used by new
terminals. Existing terminal items, themes, width preferences, grids, input
ownership and shell cwd/PIDs are retained. Empty workspaces can be edited without
spawning shells. Native xcb/Wayland checks exercise edits, cancellation, invalid
folders, empty workspaces and actual new/existing-shell cwd behavior alongside
the existing drag, input, cursor and appearance coverage.

Native Mac workspace-editing review remains open. Native folder browsing, agents,
keyboard-only moves, saved configuration and terminal lifecycle/ownership UI
remain later slices; this does not switch the production frontend.
See [editing a workspace](toolkit-ui-study.md#editing-a-workspace).


## Qt study: folder browsing

Workspace creation and folder editing now expose Browse beside the path field.
Qt Quick Dialogs supplies the platform picker, with its standard fallback when
native dialogs are unavailable. A missing typed folder starts browsing from its
nearest existing parent. Choosing fills the form only; Create/Save performs the
existing service request. Picker cancellation retains typed text and the parent
form, while cancelling the form discards an accepted choice.

Local file URL conversion preserves spaces, Unicode and URL-special characters.
Native xcb/Wayland input checks drive Qt's standard fallback independently of
host portals, exercise keyboard entry, cancellation, selection and no implicit
writes, then verify a real new shell starts in the selected Unicode folder.
The owner confirmed the native Mac picker works. Physical Linux picker review
remains open; no saved production
catalog or renderer adoption is introduced. Qt Quick Dialogs is an existing Qt
module, linked explicitly by the UI study target.
See [folder browsing](toolkit-ui-study.md#browsing-for-a-folder).


## Qt study: keyboard pane moves

Add Shift to Cmd–Option on Mac or Super–Alt on Linux: Left/Right reorder the
selected terminal, Up/Down append it to the adjacent workspace and follow focus.
Boundaries do not wrap, full destinations show a brief notice, and pending move
requests consume repeat shortcuts without sending them to a shell. Pane menus
and the scrollable shortcut overlay expose the same actions. Overview follows
keyboard moves without changing PTY geometry.

Native xcb/Wayland checks pass reordered navigation, focus following, workspace
appearance, empty-source memory, full/boundary no-ops, Overview moves, menu access,
stable grids and original-reader PID/cwd/input isolation. Native Mac keyboard
move review remains open. Folder browsing is owner-confirmed on Mac.
See [keyboard review](toolkit-ui-study.md#keyboard-review).


## Qt study: TerminalColors and Ghostty imports

Appearance now imports a local Ghostty file or a chosen TerminalColors page/direct
download link, with optional name and attribution. Import validates and saves an
offline palette, then previews the current workspace through the existing
Apply/Cancel workflow. Source/raw text/license notes are retained in a separate
configuration theme collection; no downloaded external palettes are bundled.

The existing bounded color parser is reused. Non-color settings never execute.
Exact duplicates, corrupt state, concurrent-window merges, failed/redirected or
oversized downloads and cancellation are covered without changing running shells
or existing themes. Built-ins remain usable if saved imports are invalid.

Cross-platform offline model/store/network tests and native xcb/Wayland UI checks
pass. The real Nord Ghostty export and a live TerminalColors page link passed
Qt HTTPS + parsing + disposable caching on 2026-10-18. Native Mac import acceptance
remains open; this adds palette import, not a production frontend switch.
See [imports](toolkit-ui-study.md#importing-terminalcolors-and-ghostty-themes).


## Qt study: runtime captions and agent Overview

The Qt candidate now consumes the existing read-only context and agent lifecycle
subscriptions. Pane and native-size Overview captions show directory/Git/branch/
linked-worktree context, with remote/unavailable fallbacks. Agent labels expose
Working, Needs input, Ready and Status unknown without interpreting terminal text.

The rail has an Agents entry and Overview has explicit, visibly active identity
and attention filters plus empty-result messages. Filtered previews use a cached
projection of the real catalog; terminals keep stable items, readers and grid
sizes. Drag insertion maps back to real ordinals rather than reordering hidden
terminals accidentally.

The metadata adapter is bounded, validates user-owned control sockets, deduplicates
updates and isolates malformed/disconnected/timed-out feeds from terminal input.
It does not attach PTYs, start runtimes, poll or launch Git. Native Mac review and
production adoption remain open. See
[live context and agents](toolkit-ui-study.md#live-context-and-agents).

## Session service shutdown acknowledgement

The full check exposed an immediate shutdown/reopen race: a caller could receive
the shutdown reply and connect to the still-listening socket before its service
exited, leaving a request that would never be accepted. Shutdown now closes and
retires its own endpoint and releases service ownership before acknowledging.
The existing no-replay rule for mutations remains intact. Runtime shells survive
service-only shutdown. The shutdown/reopen, lifecycle, concurrency and soak checks
pass, including an assertion that the endpoint is retired before acknowledgement.


## Runtime hardening: short input bursts

Expanded native UI tests exposed a healthy terminal losing its connection during
a short typed command. The runtime's PTY writer queue counted messages: sixteen
single-key packets could exhaust it while the writer was briefly unscheduled,
even though only a few bytes were pending.

The writer queue now retains its 1 MiB byte budget, including a packet held by a
blocked writer, while allowing up to 4096 small packets. A separate message limit
keeps descriptor memory bounded. Failed sends, receiver shutdown and completed
writes release their reservations; input remains ordered, and genuine queue
exhaustion still isolates the attached sender. No protocol or saved state changes.
Transport/event queues retain their own existing bounds.

Deterministic tests hold the writer back while queuing 2048 single-byte packets,
exercise byte/message limits and confirm budget release. The real PTY regression
sends those 2048 packets followed by 120 KiB of paste data, verifies exact ordered
bytes, then checks the same reader/attachment accepts a resize. It runs as part
of `just check`. The capability fixture also waits for its one expected request,
removing an unrelated background-thread exception from otherwise passing checks.


## Qt chrome aligned with the working viewer

The Qt study now uses the spike's bundled Adwaita Sans for interface text,
with native toolkit text rendering and separate terminal typography. Original
geometric icons replace font-dependent symbols. The compact 40-pixel top bar
and 44-pixel rail restore the left menu placement, workspace label, centered
pane-position pill, focused/count text and adjacent New terminal action.

Chrome colors follow the same light/dark mixing rules as the working viewer,
including cursor-derived accents and semantic attention colors. Menus, section
separators, check marks, disabled actions, tooltips and workspace rows use shared
controls. Long names elide; eight indicators, count and actions fit the minimum
680-pixel window. Agent attention has a rail count and pane-position dot, with
status text retained separately in the pane header. Idle previews use an arrow;
the grab cursor appears during dragging only.

This ports the approved visual direction into `just toolkit-ui`; `just spike`
remains the daily-use frontend. Native Mac visual acceptance is still required.
The UI suite checks actual bundled font selection, minimum-width non-overlap,
long workspace names, attention transitions, existing keyboard/menu/focus flows,
and stable terminal identities/geometry through overlays. Review captures include
`chrome-menu.png`, `chrome-workspaces.png` and `chrome-narrow.png` under
`.local/artifacts/qt-ui/{xcb,wayland}/`.


## Qt terminal lifecycle and ownership

The Qt study now shows a compact status card when a terminal is disconnected.
Recorded exit codes/signals, explicit stops and missing-exit evidence come from
the existing session service. Unknown evidence stays unknown. Status checks are
bounded, coalesced per terminal and request driven; they never start a missing
service, replace a shell or attach a PTY owner. Check again refreshes those facts.

- **Reconnect** attempts a normal attachment to the running process. If another
  window owns it, that window keeps control.
- **Move here…** asks for confirmation before transferring the existing process.
  The old owner receives an explicit release state and can later reconnect.
- **Start new shell** is offered only for a stopped runtime. The existing service
  revalidates that no shell is live and starts in the workspace's configured
  folder. It preserves terminal identity, theme and width; it starts a new process.
- **Close terminal…**, in the pane menu, asks before stopping the process and
  removing its catalog entry. Cancel receives initial focus; Enter cannot silently
  confirm destruction. Other terminals retain their live items, PIDs and input.

Snapshot reconnect commits a newly decoded client replica and rebuilds its render
handles without taking terminal authority from the runtime. A failed snapshot
keeps the old display; failures stay local to that terminal. Removed panes retire
their context/agent/status observers. Selection, widths and remembered workspace
focus are reconciled by stable identity rather than retaining shifted indices.

This slice also fixes returning from Overview with an obsolete animated scroll
position. Reveal uses the intended scroll target and clamps to the current
workspace; a disconnected selected terminal's controls remain reachable.

`just toolkit-lifecycle-test` exercises lifecycle-only requests, fragmented,
malformed, oversized, closed and timed-out replies, missing-service non-start and
observer retirement without a display. The native UI suite uses real runtimes to
exercise exit 42, missing evidence, new-shell recovery, explicit ownership transfer,
non-stealing reconnect, service downtime and confirmed/cancelled close. It verifies
stable sibling UI instances and reader PIDs, then the normal narrow-window layout.

For manual Mac/Linux review, run `just toolkit-ui`, enter `exit 42`, inspect the
reason and choose Start new shell. Open that pane's overflow menu, choose Close
terminal…, cancel once, then confirm. The workspace should become empty when its
last terminal closes; its neighbours should keep running. Native Mac acceptance
of this slice remains pending. Paired-window saved-session integration remains a
later slice; the study launcher still owns only private disposable sessions.

## Qt shortcut editor and sparse persistence

The Qt candidate now exposes Shortcut settings through its menu, Commands and
help overlay. Real key recording validates conflicts and reserved shell/clipboard
keys. Apply saves sparse overrides in the existing platform UI layer and updates
bindings/hints immediately; Cancel keeps live bindings unchanged. Managed files
remain read-only, and externally changed settings block stale saves. Model and
native Linux input checks cover these boundaries; native Mac recorder review,
forwarding is now implemented. Native Mac recorder/forwarding review and
saved-session integration remain next work.

## Qt one-shot terminal key forwarding

The Qt candidate now exposes Send next key to terminal through Commands, the menu
and an optional configurable `forward` binding. Armed state is visible and expires
after ten seconds. Escape, pointer/focus, pane availability and composition
changes cancel unused requests. One chord bypasses application/clipboard and
host word-motion shortcuts through the existing terminal encoder, retaining held
repeats and application-requested release reports. Native Linux real-reader and
key regression coverage validate this; Mac acceptance remains pending. Saved
session integration and persistent font/theme preferences are still ahead.

## Qt font preference persistence

Terminal text Apply now saves face/size differences from managed defaults to a
separate atomic per-platform UI file. Preview/Cancel preserve the saved choice;
Use dotfile defaults clears field overrides when applied. New launches load the
saved defaults. Existing other windows and temporary size shortcuts retain local
behavior. Invalid stores remain untouched; conflicting/failed saves are inline,
cancellable outcomes. Model and Linux native UI checks cover these boundaries.
Native Mac persistence review, workspace theme persistence and saved-session
integration remain ahead.

## Shared configuration location

Both Mac and Linux now use `$XDG_CONFIG_HOME/terminal-workbench`, defaulting to
`~/.config/terminal-workbench`. `WORKBENCH_CONFIG_DIR` remains an explicit override.
Mac startup copies missing managed config, themes and platform UI preferences
once from the former Application Support location. Existing destination choices
and symlinks win; original files and session/runtime data remain untouched.
Isolated migration tests pass; native Mac acceptance remains pending.

## Qt saved-session startup

`just toolkit-workbench` uses the existing service and saved workspace catalog;
closing or failing the viewer does not stop its shells. Existing shell PIDs,
workspace identity and stopped entries survive reopening. Live owners are not
replaced by normal attachment. The disposable study remains available separately.
Filesystem notifications and read-only service requests now update peer catalogs;
a menu action retains explicit refresh. Real-runtime launcher checks and native Linux UI
regressions cover this first slice. Saved Qt window profiles/theme choices are implemented;
broader native Mac review remains ahead.

## Qt saved window views and workspace colors

Saved Qt launches now lease independent reusable view profiles. Stable workspace
and terminal IDs restore layout, focus, scroll, Overview/filter/motion state and
committed workspace themes. Theme snapshots retain full palette/default colors
without requiring the source file to remain present. Preview/Cancel never stores
temporary colors. Restore cannot recover a shell or transfer terminal ownership.

Views save on a debounced client-state change and normal close; terminal parse
and paint paths perform no persistence work. Invalid/future/oversized/stale view
files stay untouched with a local notice. Independent slot reuse, theme roundtrip
and removed-source restoration are covered alongside native Linux restart checks
with real readers. The owner confirmed appearance retention with `f80f00b`. Broader Mac view/display
acceptance and cross-window appearance synchronization remain open.

## Qt cross-window workspace updates

Saved Qt windows now follow catalog changes through filesystem invalidation and
bounded read-only service requests, without polling or shell recovery. Stable IDs
retain local pane layout, focus, themes and live readers. Local mutations pause
background reads; failed updates keep existing workspaces and show a local notice.
Older services retain manual refresh until explicitly upgraded. Service/model and
two-window X11/Wayland checks pass; native Mac review remains open. See
[automatic workspace updates](qt-saved-sessions.md#automatic-workspace-updates).
