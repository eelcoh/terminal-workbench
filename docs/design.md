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

## Still to do in stage 1

- The inventory: the UI test already captures most screens
  (`.local/artifacts/qt-ui/<platform>/*.png`). They need to be put next to Rex
  screenshots of the same tasks, on Linux and the Mac.
