# Keyboard interaction

The first Commands palette now exposes workspace and terminal actions without
freezing individual navigation bindings. Start with actions and terminal input
requirements rather than copying a predecessor's keymap.

## Actions to design

- Open the workspace switcher; select the next or previous workspace.
- Create, close, and rename a workspace.
- Create a terminal; focus the next or previous terminal; close a terminal.
- Move or resize a pane; reveal the focused pane in the scrolling layout.
- Open the agent overview; jump to an agent awaiting input.
- Open the command palette and shortcut settings.
- Search terminal output, copy a selection, and paste.

## Niri-inspired width actions

The prototype supports **Cycle terminal width** (1/2 → 2/3 → 1/3), explicit preset
selection and **Toggle terminal full width** for the focused terminal. Full width
restores its previous preset when toggled back; other terminals keep their widths.
Commands offers each preset, Shortcut settings edits the bindings, and dragging
a right gap previews a snapped guide. Defaults are Ctrl+Shift+R/F on Linux and
Command+Option+R/F on Mac. Niri's Super+R/Super+F interactions are references; using the same
chords inside Workbench on Linux would compete with the window manager. Native
application fullscreen remains distinct from one terminal filling its content area.
See [layout requirements](ux-spike.md#niri-reference-and-next-layout-slice).

## Routing requirements

Use familiar macOS Command conventions and suitable Linux bindings. A mechanical
Command-to-Control substitution can collide with shell and terminal-app input;
evaluate Linux defaults individually.

Distinguish application-wide, focused-terminal, and modal action scopes. Define
precedence and collision handling, including an explicit way to send an
intercepted binding to the terminal.

Preserve ordinary shell input and control keys such as Ctrl-C, Ctrl-D, and
Ctrl-Z. Account for Option/Alt, IME composition, alternate keyboard layouts,
key repeat, and terminal applications with their own keymaps.

Keep shortcuts configurable and visible in menus or the command palette. Mouse
navigation must cover core actions without requiring knowledge of shortcuts.

## First spike

Begin with text input and standard clipboard actions. Add only the application
bindings needed for the spike, then verify their routing with a shell and a
full-screen terminal application on each platform.

## Clipboard conventions in the spike

| Action | Linux | macOS |
| --- | --- | --- |
| Copy selection | Ctrl+Shift+C | Command+C (unverified) |
| Paste clipboard | Ctrl+Shift+V | Command+V (unverified) |

The hamburger menu also exposes Copy and Paste. Ctrl-C remains terminal input.
Drag selects text; double-click selects a word and triple-click selects a line
through Ghostty's gesture engine. Shift forces local selection when a terminal
application has mouse reporting enabled. Typing or pasting clears selection and
returns to live output. Clipboard text is limited to 64 KiB in this spike.
Clipboard defaults can now be changed or cleared in Shortcut settings. The final
desktop input layer and full native platform acceptance remain future work.

## Workspace layout prototype

Navigation is mouse-first while shortcut design remains open. The Workspaces rail opens a drawer to select
a workspace; a pane's title or terminal area focuses it. Copy/Paste always applies
to the selected pane. Selection drags and scrollbar drags retain pointer ownership
until release; divider gestures are handled by the client and never forwarded
to terminal applications. No Herdr workspace or pane bindings were imported.

Name dialogs consume text/key input while terminals continue parsing output.
Enter saves while a text field is focused; on a button, Enter/Space activates it. Escape/Cancel dismisses without a mutation, and Ctrl+A/V (Linux) or
Command+A/V (macOS, unverified) follow text-field conventions. Saving runs
asynchronously and disables editing until completion. No dialog input reaches the
focused terminal. Workspace/terminal creation and rename remain available through
visible controls and Commands. Navigation now has editable platform defaults;
see [shortcut settings](shortcuts.md).

Stop confirmations consume all terminal input. Escape/Cancel dismisses them;
termination requires clicking Stop or explicitly focusing it with Tab/Shift+Tab
and pressing Enter/Space. Opening Enter and Space do nothing. Window Close continues
to detach without stopping any shell. No workspace close shortcut is frozen yet.

## Overview prototype

Overview opens with its header button, Ctrl+Shift+O on Linux, or Command+Option+O
on macOS (unverified). This default follows the application modifier
family and can now be changed through Shortcut settings. The chord toggles the
overview. Spatial mode is the default: Left/Right selects a terminal in its
workspace, Up/Down moves between workspaces while retaining the nearest column,
and Enter jumps. Wheel scrolls workspace rows; Shift+wheel scrolls the row under
the pointer horizontally. Visible row arrows provide the same horizontal action.
The List button selects the original compact view with Up/Down navigation.
Typing in the search field filters names in both modes; Escape/Done dismisses. Text-field select-all and paste use Ctrl+A/V or Command+A/V. A no-match
Enter does nothing. All overlay input is consumed, including paste and shortcuts,
while terminal output continues. Existing name/stop dialogs take precedence.


Spatial zoom transitions keep terminal input scoped to the overview until it
closes. A new terminal key/text event completes a closing zoom and then enters
the focused terminal without losing queued short presses. The exit gesture stays
consumed. Toggle Overview again during a closing zoom to cancel its pending jump
and reopen. Reduced motion (`WORKBENCH_REDUCED_MOTION=1`) skips transitions.

In Spatial Overview, drag a thumbnail between workspace rows or between cards.
Release over the outlined row to save. Escape cancels the drag without closing
Overview. Wheel and Shift+wheel remain available while holding the thumbnail;
hovering near the edges also reveals more rows/cards. Empty workspaces are drop
targets. A workspace with eight terminals rejects incoming moves.

## Working-view pane navigation

The configured previous/next chords focus adjacent terminals. Compact pane marks
in the top bar select a terminal directly; focused, visible and off-screen panes
have distinct marks. Scroll horizontally on a trackpad or vertically with a mouse wheel over
that strip to navigate. Fractional deltas accumulate, direction reversal resets
the accumulation, and a short cooldown bounds momentum-driven changes. Leaving
the strip clears residual navigation input.

Over terminal content, horizontal-dominant two-finger gestures now pan the
working strip directly. Fractional deltas and OS momentum are preserved with a
bounded per-event movement; no second fling is added. Vertical and vertical-
dominant gestures retain terminal scrollback/mouse reporting. Selection/scrollbar
and divider drags, menus and modal overlays keep ownership and block pane swipes.
Focus stays on its terminal while that terminal intersects the viewport, then
moves to the adjacent visible terminal. A click, position mark or navigation
shortcut reveals the chosen terminal normally. Pan position uses the existing
per-window saved scroll value and survives catalog refresh; terminal widths and
processes stay independent of viewport movement.

The target gets focus immediately. The view scrolls through the intervening panes for
270ms on adjacent moves, increasing with travel distance up to 1.25s. Retargeting
starts from the displayed position. Typing is not delayed or consumed by the
animation. Clicking or scrolling
inside moving terminal content settles the view before hit testing. Overview,
dialogs, theme selection, workspace switches and window resizing also settle the
working-view motion. `WORKBENCH_REDUCED_MOTION=1` disables both overview and pane
transitions. No new terminal-intercepting keyboard chord is added by this slice.

## Commands palette prototype

Open Commands in the hamburger menu opens a searchable action list. Ctrl+Shift+P (Linux) and
Command+Option+P (macOS, native acceptance pending) are editable default chords.
Up/Down or wheel selects, Enter/click runs, and Escape/Done closes. Disabled actions
explain their state; stop/replacement actions open the explicit confirmation dialogs.
The dispatch frame remains modal, so a key cannot also save a newly opened dialog
or reach a shell. Existing dialogs, overview and appearance retain precedence.
The palette opens from the working view only. Queries and disabled commands are
consumed locally while terminal output continues parsing.

The action registry is client-local and uses existing management workflows.
Shortcut settings now records, validates, saves, clears and restores per-action
bindings. Navigation defaults cover terminals and workspaces. Send next key to
terminal bypasses interception once through the ordinary Ghostty encoder.
See [shortcut scopes and defaults](shortcuts.md); native platform acceptance,
IME/layout behavior and multi-key sequences remain follow-up.
See [Commands](commands.md) for the action set and validation.

## Shortcut help

Command+Option+/ on Mac and Ctrl+Shift+/ on Linux open a grouped overlay using
the current configured map. It is available in the working view and Overview;
existing editors and overlays retain precedence. Escape/Done/toggle return to the
underlying view, and Enter/Edit shortcuts leads to settings. All help input is
consumed locally. Mac application defaults now share Command+Option, including
new terminal/workspace; copy/paste keep Command+C/V. Saved overrides are preserved.
See [shortcut help and validation](shortcuts.md#shortcut-help-overlay).

## Dialog and overlay focus

Tab and Shift+Tab cycle through available controls in Commands, shortcut help,
Shortcut settings/recording, Overview, Appearance and management dialogs. An accent
outline marks keyboard focus. Enter/Space activates the focused button; typing,
select-all and paste affect only a focused text field. Clicking a field returns
text focus to it. Arrow keys choose results/terminals/palettes while search or the
content area is focused. Help scroll keys work while its body is focused.

Opening a name/path editor focuses its first field. Enter there retains the quick
Save/Create action. Commands search retains Enter to run the selected result;
Appearance search/grid retains Enter to Apply. Help's body retains Enter to Edit
shortcuts. In a shortcut recorder, the capture field records chords and Enter
saves; Tab moves to Clear, Default, Cancel and Save without recording those keys.

Stop and Start new shell open with no confirmation button focused. Tab first
selects Cancel, then the confirmation; reverse traversal explicitly selects the
confirmation. Enter/Space without that selection never stops/replaces a shell.
Busy management disables traversal and activation. Escape cancels an editable
overlay, restoring the underlying view and focus; Appearance restores its entire
previous transaction. Closing a recorder returns to its settings result. Help
over Overview preserves the exact query, selection and focused control.

`just focus-gui-test` checks these flows with disposable Linux/Xvfb sessions.
The owner confirmed basic Mac focus navigation. Broader Option input, non-US
layouts, IME and accessibility remain
part of platform acceptance.

## Minimal chrome input

The rail opens Workspaces or Agents. Temporary drawers own keyboard/pointer input
until selection, Escape or outside-click dismissal; they do not resize PTYs.
Pin keeps the panel visible and changes terminal estate. Tab/Shift+Tab and arrows
navigate drawer controls. Menu items support arrows, Tab/Shift+Tab, Enter/Space,
Escape, outside-click dismissal and wheel scrolling in short windows. Their
opening, dismissal and dispatch frames stay modal so input cannot reach a shell
or accept a newly opened confirmation. [Usage and layout](minimal-chrome.md).
