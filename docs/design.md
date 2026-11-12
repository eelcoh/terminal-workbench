# Design step (v0.4)

The owner's verdict on the first polish pass: nicer, but not half as polished
and subtle as Rex. The owner particularly likes Rex's minimal design on the Mac:
not too much chrome, not too many frames (2026-11-08). The plan has three stages.

1. **Groundwork**:
   - one file of design values that every component reads;
   - a gallery of every component and surface, which the UI test can
     screenshot;
   - a screenshot inventory beside Rex doing the same tasks.
2. **Design**: a designer, or the owner with Claude, works from that inventory.
   They change the values and the components, and watch the gallery update.
3. **Judging**: each round goes into daily use for a few days before the next.

## Design values

`spikes/qt-input/Design.qml` is a QML singleton (`import Band.Design`) with:
- **type sizes:** caption 11, small 12, body 13, control 14, heading 16, title
  20, display 28;
- **spacing steps:** 2, 4, 6, 8, 12, 16, 20, 24;
- **corner radii:** 4, 6, 8, 12;
- **line widths:** 1 and 2;
- **timings:** a 120 ms fade and a 220 ms move.

Colors stay with the terminal theme (`UiThemes`, passed down as `tones`): the
chrome takes its colors from whatever theme the terminal uses. That is the main
constraint for any design.

Before this, the QML used 12 type sizes, 10 radii and about 14 spacing values.
The first pass (2026-11-08) moved every literal type size, radius, border width,
timing and spacing property to the nearest step, without redesigning anything:
- dialog titles were 18 or 22 and are now all 20;
- headings of 15–17 are now 16;
- spacing of 3 and 5 is now 4, 10 is now 12, 14 is now 16, and 18 is now 20;
- dialog radii of 10 are now 12.

Dots, badges and pills keep a radius of half their size. Positions and sizes
inside layouts (`x`, `y`, widths) still use numbers of their own; they move to
the spacing steps as the designer touches each surface.

## Gallery

Commands → **Design gallery** opens `Gallery.qml` over the main area; Escape or
Close leaves it. It shows the shared components under three terminal themes:
the workspace's own, Workbench Dawn (light) and Dracula (saturated):
- the type steps with their roles;
- buttons: normal, hover, pressed, focus, selected, disabled, primary, primary
  with focus, and icon-only;
- a popup surface with menu items: normal, hover, focus, checked, disabled;
- key chips, the accent, ready and waiting marks, the radii and the spacing
  steps.

Below the columns, **Band in place** puts the window's own parts together
under each theme with sample workspaces and terminals: the top bar
(`TopBar.qml`), the sidebar rows (`SidebarWorkspace.qml`, `SidebarPane.qml`)
and pane headers (`PaneHeader.qml`), focused and not, with an agent needing
input, uncommitted changes and a terminal's own colors. The window uses the
same components, so the gallery shows what Band shows.

Hover, pressed and focus are shown without a pointer through a `preview`
property on `ChromeButton` and `ChromeMenuItem`. The columns sit side by side
when they fit, at least 340 px each, and below each other otherwise. The UI
test opens it from Commands and saves `design-gallery.png`.

## Following Rex

- On macOS the application menu is in the system menu bar, not behind a button
  in the top bar ([minimal chrome](minimal-chrome.md)).
- In the sidebar the caret that opens a workspace's panes sits on the right of
  its name, before the remove button, so the names line up with the left edge.
- One bar at the top: the system title bar folds into Band's top bar by default
  (`integrated-title-bar`), which keeps the pane strip.
- Fewer layers in the sidebar: no panel title (the rail names the section; the
  pin sits at the foot of the rail, and Keyboard shortcuts live in the menus,
  the command menu and their chord); workspaces as small bold section headers,
  all open until closed (the current one in full color, the others muted, the
  count and activity dots only while closed, the dots right after the name);
  the caret and remove button only on hover or keyboard focus, with the hover
  background across the whole header; and each terminal on one line, the
  folder and branch muted after the name (two lines until 2026-11-11).
- One sidebar view: workspaces with their panes. The rail filters it by kind
  (Shells, Apps, Hosts, Agents; one at a time; the shown one again clears it) under All panes, which clears
  them. Saved hosts (connect and remove on hover) follow with the hosts
  filter; an agent row's details button shows on hover. The "new" actions sit
  at the foot, their chords on hover.
- On macOS every type step is a pixel smaller.
- Commands is a search field over a list, high in the window: no title, key
  legend or Done button (Escape or a click outside closes it), the context in
  the field's placeholder, and tighter rows.
- Add applications follows: the search heads it, with the app count and
  Sources beside it; no subtitle, key legend or Done.
- Appearance, New workspace, Rename, the folder and host dialogs follow the
  window's tones (they had a fixed dark palette), and every dialog puts its
  decision on the right with the main action last and highlighted; focus
  still starts on Cancel in confirmations.
- Panes are one panel in the workspace's terminal background with a hairline
  border and the header blended in; no frame marks focus (an accent frame
  until 2026-11-11). The focused pane's name is in full color, the others
  muted, and its hairline is a little brighter, tinted toward the accent, with
  a faint glow just inside (owner, 2026-11-12; a thin accent line along the
  top of the header before that); the cursor, the dim and the sidebar show
  the rest.

### First round from the inventory (2026-11-11)

The owner's Rex and Mac screenshots in the inventory led to four changes that
leave the layout alone:
- Text buttons are pills: a soft wash of the foreground at rest, deeper on
  hover, the accent for the main action. The top bar's Overview and the
  sidebar's "new" actions stay bare text (`quiet`).
- The current row (and any selected choice) is a soft pill with a thin
  outline instead of an accent fill.
- Sidebar panes take one line.
- Panes lose the accent frame and the separate header strip (above).

The top bar stays (owner, 2026-11-12). Folding it into the pinned sidebar's
top row, as Rex does, was tried and reverted: the owner prefers the top bar
with the pane markers in the middle. Not everything of Rex is better.

Still to decide, as they change the layout: the rail (Rex has none), the
sidebar's foot as small icons, one-line command rows with plain key glyphs,
settings in cards.

Rex's sidebar is a level shallower than Band's (owner, 2026-11-11): its rows
are sessions (about a Band workspace; a stacked icon when one holds several
panes), under group headers ("Base", "Onboarding"), and panes are not listed.
Band lists each pane under its workspace, and keeps that (owner,
2026-11-11): Rex's local sessions are what Band's workspaces already are, and
Band runs one session per machine. The identifier Band needs is the machine,
not the session: a small muted machine header ("This Mac", a host with its
reachability mark) above that machine's workspaces, shown only once more than
one machine is involved, which arrives with the remote runtime
([evaluation](remote-runtime.md)). Plain host panes stay in the local
workspaces.

## Still to do in stage 1

- The inventory: `just design-inventory` writes
  `.local/artifacts/inventory/index.html`, Band's UI test screenshots per task
  (first run, sidebar, filter, hosts, title bar, pane chrome, commands,
  settings, confirmations, Overview, gallery) beside two empty columns. Still
  needed: Rex screenshots (`rex/<task>.png`) and Band on the Mac
  (`macos/<task>.png`) dropped into that folder by hand.
