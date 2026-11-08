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

## Still to do in stage 1

- The gallery: a window showing every component in each state (normal, hover,
  selected, disabled, focus) under a light, a dark and an unusual terminal
  theme. The pane header, the sidebar rows and the top bar live inside
  `Ui.qml` and `NavigationPanel.qml`, so they must become components before the
  gallery can show them.
- The inventory: the UI test already captures most screens
  (`.local/artifacts/qt-ui/<platform>/*.png`). They need to be put next to Rex
  screenshots of the same tasks, on Linux and the Mac.
