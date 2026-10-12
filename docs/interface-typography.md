# Interface typography and surface polish

Workbench now uses bundled Adwaita Sans Regular, an Inter-derived interface face,
for sidebar labels, pane chrome, fields, Commands, Overview, Appearance and help.
Terminal content, shortcut keycaps and palette terminal samples retain JetBrains
Mono. No system fonts or font downloads are needed on Linux or Mac. The original
font bytes, package provenance, digest and OFL attribution are recorded in
[fonts/adwaita-sans](../fonts/adwaita-sans/README.md).

The interface scale has five roles: captions 12, body/control labels 14, leading
text 16, dialog titles 22 and sheet titles 24 logical pixels. Titles have a clear
step above field labels and captions; action descriptions remain secondary.
Name/path fields use 16-pixel text. Keycaps keep a compact monospace face.
Spacing stays within the existing field, row and button hit areas so navigation
and narrow-window workflows remain familiar.

Elevated sheets use a consistent 12-pixel corner radius; smaller cards use eight
and buttons six. Buttons show a border on hover and accent feedback when pressed.
Unavailable controls have a quieter background and lower text emphasis. Final
Save/Create/Apply actions have an accent outline; Stop confirmations have a
destructive-color outline. Keyboard focus remains a separate outer accent ring.
These are shared presentation helpers, not runtime/session fields.

## Backing resolution

The earlier workspace renderer loaded a Retina terminal atlas but rendered it
into logical-size pane textures, then enlarged those textures for display. That
introduced a reduction/enlargement stage that could soften Mac terminal text.
Working-pane and navigation textures now use the window's backing-pixel ratio.
A camera maps existing logical drawing coordinates into those pixels, and native
texture presentation snaps positions to backing pixels. Help's retained body
texture follows the same ratio, including scissor clips.

Terminal font choice, 16-pixel logical size, cell grid, PTY geometry, mouse/input
coordinates and selection behavior stay independent of interface typography.
The default grid remains 7×16 at ordinary and doubled raster resolution.
Overview thumbnails intentionally retain their bounded 384×240 preview cache;
they are miniature views rather than a terminal's reading surface.

`WORKBENCH_RENDER_SCALE=2` is a diagnostic override for exercising doubled
backing textures on a 1x Linux display. `just retina-gui-test` runs the same
focus/workspace/stop/recovery flows at 1x and 2x, compares logical PTY geometry,
checks actual backing allocations and font loading, then exercises 15-terminal
navigation at 2x. This checks the rendering contract; it does not establish
native Retina appearance or physical trackpad behavior. Native Mac sharpness
and mixed-DPI monitor transitions remain part of owner acceptance.

## Pane headers and workspace sidebar

The owner reported that the typography/sharpness pass was much better. The next
chrome refinement keeps the same pane dimensions, sidebar rows and action hit
areas, while giving selection and metadata a clearer hierarchy.

Workspace rows use a rounded selected surface, an inset accent marker and more
readable inactive names. A small number shows the saved terminal count, including
stopped or elsewhere-owned terminals. The existing unread dot remains separate.
A quiet divider groups workspace actions below the list. Rename, Folder,
Appearance and Commands keep visible text labels with a hover/pressed surface;
New workspace and New terminal retain their filled controls.

Pane headers give the terminal name primary emphasis and agent identity a
secondary caption. Working, Needs input, Ready and Unknown use separate semantic
status pills when space permits. Closed/connecting states retain honest feedback
and the existing Move here, Reconnect, Check again and Start new shell actions.
A subtly tinted header and accent marker identify the focused pane. Stop and
Rename stay visible as quiet text controls, with their original hit areas.

Long names end with an ellipsis at a complete UTF-8 character boundary. Status
gets space first, then agent identity where it fits, and the terminal name uses
the remainder. Tiny closed headers can omit the status pill; recovery controls
and the existing explanatory footer stay available. Full names remain stored
and appear in name editors. No navigation bindings or input routes change.

The ellipsis helper uses one bounded pass over a short label, with narrow font
metrics, a stack buffer and no heap allocation. Chrome uses metadata already
present in the client; it adds no process, filesystem or terminal-snapshot work
to render loops. The shared session protocol and persisted view format are
unchanged. Native Mac review of this chrome slice remains pending.
