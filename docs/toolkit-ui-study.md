# Qt live UI study

Run `just toolkit-ui` on Linux or Mac. It opens six disposable shells split
between two named workspaces, plus an empty workspace. Closing the window
stops these six shells. It does not open the saved Workbench catalog. The
existing `just spike` application remains the daily-use viewer; Qt adoption
has not been decided.

The slice implements the approved minimal rail/top bar, workspace drawer,
focus/visible/offscreen pane indicators and small pane corner radius. Overview
shows live scaled terminal surfaces grouped by workspace. Click a preview to
return to that terminal, or use Done/Escape. Its geometry is a visual transform:
terminal grids and processes do not change merely because Overview opened.
Workspace names and terminal labels are fixture labels, not live cwd/Git metadata.

Width changes affect the selected terminal only; full width restores its prior
preset. Terminal geometry changes once when the width preference changes,
while positions animate independently. Horizontal wheel/trackpad deltas pan the
strip; vertical terminal input retains terminal scrollback. Overview can scroll
vertically. Physical trackpad smoothness and Mac visuals require owner review.
The application menu offers Reduced motion and separated command groups.

## Keyboard review

Use **Cmd–Option** on Mac or **Super–Alt** on Linux:

| Key | Action |
| --- | --- |
| Left / Right | Previous / next terminal in this workspace |
| Up / Down | Previous / next workspace, restoring its focused terminal |
| O | Enter / leave Overview |
| R | Cycle selected width: half → two thirds → one third |
| F | Toggle full width / restore |
| / | Shortcut help |

Escape closes Overview/overlays and otherwise reaches the shell. Copy/paste
retain the platform bindings proved by the Qt terminal probe. The top bar
indicators, workspace drawer, pane layout menu and help button provide mouse
access. Bindings are fixed for this isolated study; dotfile shortcut settings
and the production command registry are not connected yet.

## Evidence and next scope

`just toolkit-ui-test` drives real native keyboard/pointer input against six
raw PTY readers on private xcb and native Wayland (DISPLAY unset for Qt).
It checks native horizontal wheel scrolling without focus/grid changes, exact
routing/no leaked application shortcuts, independent width
changes and full-width restoration, workspace focus memory, help/Escape,
empty workspace, Overview preview activation, stable grids through Overview
and unchanged reader PIDs. Offscreen columns and background workspaces consume
new output without rasterizing rows, then show current content on reveal.
Artifacts are `.local/artifacts/qt-ui/{xcb,wayland}/`, including working and
Overview captures and opt-in disposable-terminal reports. Reports are never
created by ordinary interactive runs.

Native Mac UI execution is unverified. This slice does not yet implement
workspace/terminal creation, rename/folder editing, agents, theme selection,
drag/reorder, persistence, configured shortcuts or session ownership workflows.
It avoids dead controls for those actions. They remain later design slices,
along with typography, motion, accessibility and cache/backing-surface budgets.
Review this interaction slice before expanding it into the production frontend.
