# Minimal chrome study

The owner's Niri, Dank Material and Foot screenshots motivate a structural
reduction of permanent controls. Pane position is the requested top-bar signal.
The left side still needs both workspace navigation and cross-workspace agent
attention, with a design that can be evaluated independently of Herdr's layout.

Open [the interactive sketch](prototypes/minimal-chrome.html) in a browser.
It runs locally without a server or network requests. The review buttons switch
light/dark chrome, compact/expanded navigation, sample agent state and platform
shortcut labels. User screenshots stay in the private debug folder.

The owner approved the overall visual direction and requested a small increase
in pane corner radius. The sketch now uses 4px corners, clipping header and
terminal backgrounds to the same outline. The native client now implements this direction; see the usage and validation notes below.

## Proposed hierarchy

| Surface | Purpose | Visible controls |
| --- | --- | --- |
| Top bar | Location within the current workspace | Workspace name, pane indicators, focused pane/count, new terminal, Overview, application menu |
| Left rail | Choose a navigation task | Workspaces, Agents with Needs input count, shortcut help |
| Left panel | Find a destination | Workspace names/counts or agent names/status with workspace and terminal context |
| Pane header | Identify the terminal and its activity | Name, agent identity/state, overflow menu |

Pane indicators use a longer accent mark for focus, softer marks for other panes
intersecting the viewport, and quiet marks for off-screen panes. Clicking selects
and reveals a pane; scrolling updates visibility independently of focus. The browser study exposes names
and state through tooltips and accessible labels; native hover shows the name,
with state text retained in headers and the Agents view. Needs input has
an additional attention mark, backed by a textual state in the pane header and
Agents view. This remains a per-window view of the current workspace.

The compact rail opens a temporary drawer. Pinning it keeps a named panel visible
and reduces the terminal viewport. Both variants are in the sketch for review;
the native implementation starts compact and retains the pin option for daily-use
review. Workspaces and Agents are separate modes in that same left area. The agent directory shows
agents across workspaces with enough context to jump to the right terminal,
and offers an explicit Needs input filter. Unknown remains an honest state.
The attention count covers all workspaces, not just the selected one.

Occasional actions move into menus: workspace folder/name, Appearance, Commands,
shortcut settings and clipboard actions in the application menu; rename, theme,
width presets and Stop in the pane menu. Overflow stays visible rather than
requiring hover discovery. New terminal and Overview remain immediately
accessible. Stop keeps explicit confirmation. Theme choices for terminals stay
independent of chrome, as the alternating sample backgrounds illustrate.

## Interactive study and limits

Try selecting a far-away pane, scrolling without changing focus, selecting an
agent in another workspace, filtering Needs input, pinning/collapsing the panel,
opening an empty workspace, changing a pane width and opening both menus.
Tab/Shift+Tab traverses controls; arrow keys navigate menu items and pane
indicators; Escape dismisses overlays. Reduced-motion preferences disable smooth
scrolling. Sample stop requires explicit confirmation focus.

The HTML is a design study with illustrative terminal text and sample agent
states. Rename, folder, ownership/move, Commands and shortcut-editor entries
explain their existing application workflow instead of reproducing it. Its
Overview is a navigation sketch rather than the native spatial preview renderer.
It does not run processes, validate agent detection, change saved sessions or
implement the native application menus. Mac/Linux labels demonstrate the
existing modifier families; the sketch does not intercept application chords.

The owner approved the interactive sketch. The native client implements its
structure using the existing renderer and action registry. Native Mac visual and
input acceptance remains pending.

## Native implementation sequence

1. Review compact and pinned navigation, pane indicators and menu placement with
   the owner. Resolve how much agent information should remain continuously visible.
2. Add shared popup-menu presentation and keyboard focus, reusing the existing
   action registry and confirmation dialogs. Preserve configured Cmd+Option on
   Mac and Ctrl+Shift on Linux, including navigation and shortcut help; keep
   platform copy/paste behavior.
3. Replace the existing working-view position controls with pane indicators,
   preserving continuous scrolling, focus/visibility distinction, independent
   width presets, full-width action and reduced motion.
4. Add collapsible/pinned workspace and agent navigation using existing catalog
   and agent facts. Collapse/pin/focus is client presentation state. Persist any
   new view preference as optional compatible data, not shared runtime identity.
5. Validate input scopes and dismissal: clicking a menu must not type into a PTY;
   Escape must restore focus; opening/closing temporary drawers must not resize
   running terminals. Pinning changes the actual viewport and needs one committed
   geometry update. Keep menu changes from obscuring ownership/recovery controls.
6. Review dark/light, narrow windows, long names, empty workspaces, all eight pane
   indicators, busy/unknown agent states and multiwindow ownership. Run relevant
   navigation, management, focus, Retina and multiwindow checks, then native Mac
   and Linux trackpad/keyboard review before closing the UX gate.

No toolkit decision follows from this HTML study. The implementation should use
bounded metadata reads for visible indicators and avoid runtime inspection or
terminal snapshots in presentation loops.

## Native usage

Run `just spike`. The top bar shows the current workspace, compact pane position
marks and focused/count text, plus New terminal and Overview. Hover a mark for
the terminal name; click it or use the existing navigation chords. Marks update
visibility during continuous scrolling. The rail's stacked-rectangles icon opens
Workspaces; its activity icon opens Agents. The badge counts observed Needs input
agents across workspaces. The question mark opens the configured shortcut sheet.

A temporary drawer overlays the terminals and owns input until a selection,
outside click or Escape. It never changes PTY geometry. Pin keeps the panel
visible and changes terminal estate; Hide collapses it. Tab/Shift+Tab and arrows
navigate drawer controls, and Enter/Space selects. Scrolling reveals additional
rows. Agent rows include workspace and terminal context; selecting one reveals
that terminal. All agents/Needs input are explicit filters. Unavailable and
elsewhere terminals retain honest connection labels.

The hamburger menu reuses Commands actions for create/rename/folder/stop,
Appearance, shortcut settings/help and copy/paste. Close window detaches sessions.
Each pane's visible three-dot menu offers rename, terminal-scoped Appearance,
width presets/full width, Move to workspace and confirmed Stop. Direct ownership
and recovery buttons stay visible for closed panes. Menus support arrows,
Tab/Shift+Tab, Enter/Space, Escape, outside-click dismissal and wheel scrolling in
short windows. Assigned shortcut labels come from saved bindings; unassigned
bindings leave the hint empty. Stop and recovery retain their deliberate
confirmation focus. Native navigation and copy/paste defaults are unchanged.

Per-window version-3 view files save pin state, navigation mode and Needs input
filter. Readers retain version-1/version-2 layout and width compatibility; older
files default to the compact rail. Temporary drawers and menus are not restored.
This changes client view data only. Runtime/session protocols are unchanged.

The compact 1240×760 half-pane layout retains the 7×16 terminal cell size, with
41 rows and 82 columns. The extra columns come from replacing the 184px sidebar
with a 44px rail. Panel pinning intentionally changes usable columns. Rounded
corners mask only four presentation pixels; canvas and PTY geometry stay
rectangular. Fractional width rounding can move a neighbor by one pixel and cross
a cell boundary without changing its preferred fraction.

Linux validation is recorded in the UX spike. The browser sketch remains a
sample-data reference; native Overview uses the existing spatial preview flow.
