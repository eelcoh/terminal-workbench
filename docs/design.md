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
- Fewer layers in the sidebar: no panel title (Keyboard shortcuts live in the
  menus, the command menu and their chord); workspaces as small bold section headers,
  all open until closed (the current one in full color, the others muted, the
  count and activity dots only while closed, the dots right after the name);
  the caret and remove button only on hover or keyboard focus, with the hover
  background across the whole header; and each terminal on one line, the
  folder and branch muted after the name (two lines until 2026-11-11).
- One sidebar view: workspaces with their panes. No rail (owner, 2026-11-12;
  a 44 px icon column on the left before that): kind icons at the panel's top
  filter it (Shells, Apps, Hosts, Agents; one at a time; the shown one again
  clears it), and the sidebar toggle at the top bar's left is All panes: it
  clears the filters, or shows and hides the sidebar, and carries the count
  of agents that need input. The pin sits at the panel's foot, on the right. Saved hosts (connect and remove on hover) follow with the hosts
  filter; an agent row's details button shows on hover. The "new" actions sit
  at the foot as a row of small icons, each the kind's glyph with a plus,
  their names and chords in the tooltip (owner, 2026-11-12; a column of text
  actions before that).
- On macOS every type step is a pixel smaller.
- Commands is a search field over a list, high in the window: no title, key
  legend or Done button (Escape or a click outside closes it), the context in
  the field's placeholder, and one-line rows (owner, 2026-11-12): the title,
  then muted after it the reason a command is unavailable or the highlighted
  row's detail, and the shortcut as plain glyphs ("⌘⇧P", "Super+Alt+→").
  Menus and tooltips show shortcuts the same way; Welcome's keys page and
  Keyboard shortcuts keep the key chips.
- Add applications follows: the search heads it, with the app count and
  Sources beside it; no subtitle, key legend or Done.
- Appearance, New workspace, Rename, the folder and host dialogs follow the
  window's tones (they had a fixed dark palette), and every dialog puts its
  decision on the right with the main action last and highlighted; focus
  still starts on Cancel in confirmations.
- Settings in cards (owner, 2026-11-12): Configuration, Terminal text and
  Shortcut settings group their rows on soft rounded panels with hairlines
  between them, under a heading (`SettingsCard`, `SettingsRow`).
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
  hover, the accent for the main action. The top bar's Overview stays bare
  text (`quiet`).
- The current row (and any selected choice) is a soft pill with a thin
  outline instead of an accent fill.
- Sidebar panes take one line.
- Panes lose the accent frame and the separate header strip (above).

The top bar stays (owner, 2026-11-12). Folding it into the pinned sidebar's
top row, as Rex does, was tried and reverted: the owner prefers the top bar
with the pane markers in the middle. Not everything of Rex is better.

The sidebar's foot as small icons, one-line command rows with plain key
glyphs, settings in cards and no rail followed (owner, 2026-11-12), to be
judged in use like the rest; any of them can be reverted, as the top-bar fold
was. That leaves no layout item from the first round open.

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

### Second round from a Mac screenshot (2026-11-12)

Five changes from the owner's screenshot of Band on the Mac, built there but
not yet looked at in the running app or the gallery, and not run through the
UI test (Linux only):
- Terminal text starts 16 px inside the pane (12 before) and keeps at least
  8 px below the last line; the header's icon and the top bar's workspace
  name sit on the same left edges as the text and the pane.
- A sidebar row's detail gives way a whole part at a time: "folder · branch"
  drops the branch before the folder is cut short.
- Rows and icon buttons are rounded rectangles like fields, and the current
  one is a wash without an outline (a pill with an outline before). Text
  buttons stay pills; a selected choice among them keeps its outline.
- Search and filter fields (`ContextSearch`) are a soft wash without a border;
  the accent line appears with keyboard focus.
- A third text tone, `faint` (half way between background and foreground),
  for sidebar details, a workspace's branch and the place in an unfocused
  pane's header; the pane's name is medium weight.

### Third round (2026-11-13)

The owner installed the second round on the Mac and kept it. Five more, built
and packaged there (the bundle check loads the client); not yet looked at in
the running app, and not run through the UI test (Linux only):
- A host pane whose place only repeats its name ("syros syros") shows the
  name alone, in the header and the sidebar.
- The focused pane's hairline is a quarter of the way to the accent (0.4
  before): with the rest softer it had become the hardest line in the window.
- The sidebar's kind filters, "new" icons and pin sit 18 px in (14 before),
  their glyphs on the rows' left edge.
- Small state changes fade over 120 ms (`Design.fade`, zero under reduced
  motion): a button's hover, press and selection, a search field's wash, the
  focused pane's hairline and name, the pane actions, and the top bar's
  markers. The dim fades in on the pane left behind and clears at once on the
  pane taking focus.
- Floating surfaces (menus, dialogs, the command menu, tooltips) cast a soft
  shadow (`SurfaceShadow`: stacked translucent rounded rectangles, no effects
  module).

Seen on the Mac afterwards (owner: it works). Unpinning the sidebar made it
blink away and back, the drawer fading in where the pinned panel had been;
it now opens in place and only the scrim fades in.

### Native chrome on macOS: the system material (2026-11-13)

The owner doubts a Qt client can reach the best Mac experience and asked
about a separate native client. Before that, native chrome inside the Qt
client: the first piece is the system's sidebar material
(`NSVisualEffectView`, behind-window blending) under the top bar and the
pinned sidebar, added by the macOS adapter (`bandSetWindowMaterial` in
`native/macos/window_title.mm`). The window is transparent on macOS, Band
paints the main area opaque (`windowFill`), and the top bar and sidebar are
their theme color at 50% (`Design.materialTint`; 72% at first, hardly visible) over the material, which
follows the terminal theme's light or dark. AppKit makes it opaque under
Reduce Transparency. Linux is unchanged: its adapter reports no material and
the window keeps its opaque color.

The owner saw it in the running window on 2026-11-13 and kept it at 50%.
Open points: the cost of behind-window blending while terminals repaint is
unmeasured. It reaches into AppKit's frame view to place the material under
Qt's view, which AppKit does not promise to keep working.

Added on 2026-11-14, built and packaged on the Mac but not yet seen in a
running window:

- The unpinned sidebar drawer shows the material too. The material lies under
  the whole Qt view, so the main area and the scrim must not paint where the
  drawer is: `WindowCutout` (`spikes/qt-input/window_cutout.cpp`) clears that
  rectangle, a transparent rectangle drawn without blending. A separate small
  window confirmed on the Mac (Metal) that it removes opaque, translucent and
  clipped items below it and leaves the ones above. It relies on the scene
  graph's depth buffer, the default renderer's behavior rather than a
  documented promise. It is only shown while the material is on, so Linux
  never draws it.
- `window-material = false` in `[view]` or `[macos.view]` paints the top bar
  and sidebar opaque again, live: the adapter hides its material view and
  Band fills the whole window. Settings shows the state on macOS.
- Menus are the system's on macOS: `ChromeMenu` asks Qt for a native popup
  (`Popup.Native`, an `NSMenu`), so the pane menu has the system's shape,
  animation and keyboard handling. A native item shows its text, check mark
  and enabled state only: the key hints and Band's tones are gone there, as
  in the menu bar. Linux keeps the drawn menu. Unverified: that closing a
  native menu still returns focus to the terminal (`onClosed`).
- Icons are SF Symbols on macOS: `bandSystemSymbol`
  (`native/macos/system_symbol.mm`) maps Band's icon names to system symbols
  and draws them in the icon's color; `ChromeIcon` uses that image when there
  is one. Names without a symbol keep Band's drawing ("filter-off" and the
  Linux window buttons), and the "-add" icons keep Band's small plus beside
  the system symbol. All icons were rendered to one image on the Mac and
  looked right there; in the running window they are unseen. Linux has no
  symbol set and draws as before.

### The accent means focus and selection only (2026-11-14)

The accent also marked uncommitted changes (the dot in a pane header, a
sidebar pane row and a collapsed workspace row) and a working agent's sidebar
icon, so one color carried three meanings. Now:

- accent: the focused pane, the selected row or marker, keyboard focus, the
  main action, text selection and drop targets;
- `waiting` (amber): something needs you; `ready` (green): an agent is done;
- `changed` (a new tone, the muted text color): uncommitted changes;
- a working agent's icon is the plain foreground, a step brighter than a
  shell's muted icon.

The gallery shows the fourth tone. Built and packaged on the Mac; unseen in a
running window and on Linux.

### The top bar's markers are a small map of the strip (2026-11-14)

The markers were equal pills, the focused one wider. Now a marker's width is
its pane's share of the window (a third 13 px, a half 16, two thirds 19, full
24, in the same 28 px slot), so the row reads as the strip in small. The
focused marker is the accent and a little taller. A pane that needs input is
amber and one whose agent is ready is green, on screen or not; the others are
muted on screen and fainter off it, as before. The focused marker keeps the
small amber dot when it needs input, since its own color is the accent.

The gallery's top bar shows the four widths and three states. Built and
packaged on the Mac; unseen in a running window and on Linux.

### The app icon (2026-11-14)

Band had no icon of its own. The owner chose "the strip" from four sketches
(the strip, a prompt on a band, stacked bands, a letter B): panes side by
side that run past both edges of a near-black tile, the focused one amber
with a prompt, the same picture as the top bar's markers. Amber is the
brand color for now; inside the app the accent still comes from the terminal
theme, and amber there means "needs input", which the owner has not yet
weighed. See [packaging](packaging.md#the-app-icon) for how it is made and
shipped. The icon is not yet shown inside the app (About, the first-run
page).

## Still to do in stage 1

- The inventory: `just design-inventory` writes
  `.local/artifacts/inventory/index.html`, Band's UI test screenshots per task
  (first run, sidebar, filter, hosts, title bar, pane chrome, commands,
  settings, confirmations, Overview, gallery) beside two empty columns. Still
  needed: Rex screenshots (`rex/<task>.png`) and Band on the Mac
  (`macos/<task>.png`) dropped into that folder by hand.
