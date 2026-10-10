# Keyboard interaction

Workspace and navigation bindings are intentionally undecided. Start with actions and terminal input
requirements rather than copying a predecessor's keymap.

## Actions to design

- Open the workspace switcher; select the next or previous workspace.
- Create, close, and rename a workspace.
- Create a terminal; focus the next or previous terminal; close a terminal.
- Move or resize a pane; reveal the focused pane in the scrolling layout.
- Open the agent overview; jump to an agent awaiting input.
- Open the command palette and shortcut settings.
- Search terminal output, copy a selection, and paste.

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

The header also exposes Copy and Paste. Ctrl-C remains terminal input.
Drag selects text; double-click selects a word and triple-click selects a line
through Ghostty's gesture engine. Shift forces local selection when a terminal
application has mouse reporting enabled. Typing or pasting clears selection and
returns to live output. Clipboard text is limited to 64 KiB in this spike.
These conventions do not freeze the workspace keymap; configurable action
routing and a final desktop input layer remain future work.

## Workspace layout prototype

Navigation is mouse-first while shortcut design remains open. The sidebar selects
a workspace; a pane's title or terminal area focuses it. Copy/Paste always applies
to the selected pane. Selection drags and scrollbar drags retain pointer ownership
until release; divider gestures are handled by the client and never forwarded
to terminal applications. No Herdr workspace or pane bindings were imported.

Name dialogs consume text/key input while terminals continue parsing output.
Enter saves, Escape/Cancel dismisses without a mutation, and Ctrl+A/V (Linux) or
Command+A/V (macOS, unverified) follow text-field conventions. Saving runs
asynchronously and disables editing until completion. No dialog input reaches the
focused terminal. Workspace/terminal creation and rename remain available through
visible controls; navigation shortcuts are still undecided.

Stop confirmations consume all terminal input. Escape/Cancel dismisses them;
termination requires clicking Stop. Enter cannot confirm. Window Close continues
to detach without stopping any shell. No workspace close shortcut is frozen yet.

## Overview prototype

Overview opens with its header button, Ctrl+Shift+O on Linux, or Command+Shift+O
on macOS (unverified). This provisional action follows the clipboard modifier
conventions; the final configurable keymap remains open. The chord toggles the
overview. Spatial mode is the default: Left/Right selects a terminal in its
workspace, Up/Down moves between workspaces while retaining the nearest column,
and Enter jumps. Wheel scrolls workspace rows; Shift+wheel scrolls the row under
the pointer horizontally. Visible row arrows provide the same horizontal action.
The List button selects the original compact view with Up/Down navigation.
Typing filters names in both modes; Escape/Done dismisses. Text-field select-all and paste use Ctrl+A/V or Command+A/V. A no-match
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

Prev/Next focuses adjacent terminals. Numbered positions in the same header strip
select a terminal directly; the selected position and visible pair have distinct
marks. Scroll horizontally on a trackpad or vertically with a mouse wheel over
that strip to navigate. Fractional deltas accumulate, direction reversal resets
the accumulation, and a short cooldown bounds momentum-driven changes. Leaving
the strip clears residual navigation input. Scrolling inside terminal content
continues to reach terminal scrolling and mouse-aware applications.

The target gets focus immediately. Revealing a new pair slides cached pixels for
270ms for adjacent moves and 420–580ms for full-viewport long jumps; typing is
not delayed or consumed by the animation. Clicking or scrolling
inside moving terminal content settles the view before hit testing. Overview,
dialogs, theme selection, workspace switches and window resizing also settle the
working-view motion. `WORKBENCH_REDUCED_MOTION=1` disables both overview and pane
transitions. No new terminal-intercepting keyboard chord is added by this slice.
