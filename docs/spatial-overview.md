# Spatial workspace overview

Overview now opens a spatial stage: each workspace has a horizontal row of
terminal thumbnails. This draws on the zoomed-out workspace model of
[Niri Overview](https://niri-wm.github.io/niri/Overview.html). The prototype uses
live retained terminal screens and keeps the existing search and agent filters.

- Click a thumbnail or press Enter to enter its existing terminal.
- Drag a thumbnail to another workspace or between cards to move/reorder it.
  Empty workspaces remain visible as drop targets. Release to save; Escape or
  releasing outside a row cancels. Full destinations reject the move.
  Wheel/Shift+wheel and hovering near stage edges reveal offscreen targets.
  While dragging, all workspaces become targets even with a filter active.
- Left/Right selects within a workspace; Up/Down selects the nearest matching
  column in another workspace. Keyboard navigation reveals offscreen cards.
- Wheel scrolls workspace rows. Shift+wheel scrolls the row under the pointer
  horizontally; its < / > buttons provide an equivalent mouse action.
- Type to filter workspace, terminal or recognized agent names. Agents and Needs
  input work in both modes. Only matching workspaces remain in the filtered view.
- List switches to the compact list; Spatial returns to thumbnails. View choice
  lasts for the current window. Escape/Done cancels without changing terminal focus.

The provisional opening shortcut stays Ctrl+Shift+O on Linux and Command+Option+O
on macOS (unverified). Super+O stays available to the compositor. Thumbnail clicks,
search text, navigation, paste and scrolling are consumed by the overview.

Previews show the retained viewport, including its scroll position and selection;
they do not force a terminal back to live output. A tiny preview helps recognize
an application, rather than providing a readable full terminal. Status and names
remain readable outside it. Disconnected terminals retain their last screen with
a Disconnected label. Empty workspaces show a drop area instead of terminal cards.

Opening the spatial stage never changes PTY geometry. Window resizing while it
is open also leaves geometry alone; returning to the working view applies that
view's normal layout. All sessions keep parsing output, including hidden ones.
Visible preview textures refresh when dirty at most ten times per second, and
unchanged frames reuse them. Offscreen cards neither allocate nor render previews.
Each cached preview is 384x240; caches are released when a pane/window closes.
At the 64-pane catalog limit their color textures total about 23 MiB, plus depth
buffers and ordinary terminal/renderer resources. No new runtime protocol is used.

Linux validation uses an isolated display and fifteen native shell sessions. It
checks a hidden screen's live pixel changes, process/geometry preservation,
modal input isolation, search, both scroll axes, keyboard navigation and minimum
window size. Mac rendering/input still needs validation. Touchpad gestures and a configurable zoom level remain future work.

## Zoom transitions

Spatial Overview uses a 180 ms eased zoom and fade. Opening shrinks visible working
panes toward their cards; entering a terminal enlarges that card toward its working
position. A cancellation returns to the original focus. A second Overview toggle
can reverse a closing transition and cancel a pending jump.

Terminal navigation, focus handoff and ordinary-layout resizing happen when the
closing transition finishes. New typing or a terminal key finishes it immediately,
then the queued input goes to the destination terminal; short presses and Unicode
text queues are preserved. The Enter, Escape or click that exited Overview stays
consumed by the modal. Output parsing and status updates continue throughout.
Window resizing settles an active transition to avoid stretching an old card crop.

To disable animations, launch with `WORKBENCH_REDUCED_MOTION=1 just spike`.
This also avoids allocating the extra full-window transition texture. The compact
List view changes immediately in either mode. Native OS reduced-motion preference
integration remains open; macOS animation/input behavior is not yet verified.

Pure C tests cover bounded easing, reversals, stalled frames, reduced motion and
queued input preservation. Isolated fifteen-terminal GUI tests cover both motion
modes, a reversed pending jump, immediate typing, unchanged PTY geometry and
ordinary overview navigation. Transition screenshots are in .local/test-results.


## Saved terminal moves

Dragging starts after eight pixels of movement, keeping ordinary clicks available.
The moving thumbnail reuses the existing preview. A destination outline and insertion
line show placement; red marks a full workspace (eight terminals). The overview
consumes input throughout the gesture and asynchronous save. Errors appear in its
footer, and the viewer waits for a successful save before changing ownership.

The catalog helper moves the existing terminal ID and name; its socket path does
not change, and it starts/stops no sessions. The viewer finds existing panes across
all workspaces and retains their connection, parser, selection, status and cached
textures. The source workspace remains available when its last terminal moves.
The moved card is selected and revealed after saving. Placement survives reopening.

Linux tests cover empty and populated destinations, insertion/reordering, filtered
full-workspace rejection, Escape/outside cancellation, saved placement after reopening,
and unchanged shell PID, variables, socket identity and PTY geometry during moves.
A C catalog test verifies pane pointer reuse with no reconnects or resource frees.
macOS drag behavior still needs native validation.
