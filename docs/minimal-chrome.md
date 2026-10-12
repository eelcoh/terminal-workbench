# Minimal chrome study

The owner's Niri, Dank Material and Foot screenshots motivate a structural
reduction of permanent controls. Pane position is the requested top-bar signal.
The left side still needs both workspace navigation and cross-workspace agent
attention, with a design that can be evaluated independently of Herdr's layout.

Open [the interactive sketch](prototypes/minimal-chrome.html) in a browser.
It runs locally without a server or network requests. The review buttons switch
light/dark chrome, compact/expanded navigation, sample agent state and platform
shortcut labels. User screenshots stay in the private debug folder.

## Proposed hierarchy

| Surface | Purpose | Visible controls |
| --- | --- | --- |
| Top bar | Location within the current workspace | Workspace name, pane indicators, focused pane/count, new terminal, Overview, application menu |
| Left rail | Choose a navigation task | Workspaces, Agents with Needs input count, shortcut help |
| Left panel | Find a destination | Workspace names/counts or agent names/status with workspace and terminal context |
| Pane header | Identify the terminal and its activity | Name, agent identity/state, overflow menu |

Pane indicators use a longer accent mark for focus, softer marks for other panes
intersecting the viewport, and quiet marks for off-screen panes. Clicking selects
and reveals a pane; scrolling updates visibility independently of focus. Names
and state are available through tooltips and accessible labels. Needs input has
an additional attention mark, backed by a textual state in the pane header and
Agents view. This remains a per-window view of the current workspace.

The compact rail opens a temporary drawer. Pinning it keeps a named panel visible
and reduces the terminal viewport. Both variants are in the sketch for review;
the default and whether to retain both are still design decisions. Workspaces
and Agents are separate modes in that same left area. The agent directory shows
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

JavaScript syntax and local asset/reference checks have passed. Browser visual
review and native implementation acceptance remain pending.

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
