# UX spike brief

## Goal

Approach the beauty and feel of [Dank Linux / DankMaterialShell](https://danklinux.com/)
while retaining Foot-inspired simplicity and a terminal-first working view.
This is a quality requirement from the owner. The existing prototype proves
interactions; its current appearance is a starting point.

The spike produces a runnable slice and informs the final UI toolkit decision.
No toolkit is selected solely because it can reproduce a static screenshot.

## Niri reference and next layout slice

[Niri](https://niri-wm.github.io/niri/) is an explicit interaction reference:
scrollable tiling, focus/reveal behavior, polished overview entry/exit and simple
controls. The owner considers current scrolling/navigation close to the desired
feel. Preserve that progress while improving variable-width navigation and the
overview's continuity. The owner's impression of Material-inspired widgets is
a visual direction, not a claim about Niri's widget implementation. Dank Linux
remains the visual quality reference; Foot remains the simplicity reference.

### Problem in the current prototype

The working view shows at most two terminals. `WorkspaceClient.divider` stores
one arbitrary left/right ratio for the whole window, persisted in its view slot.
Dragging it changes every workspace's layout and every subsequent terminal pair:
the left position stays narrow even when a different terminal occupies it.
This does not support three equal terminals or independently sized columns.

### Required behavior

- Each terminal has its own preferred width in the scrolling workspace strip.
  Width belongs to terminal identity, not a left/right slot or a particular pair.
  New terminals start at 1/2 width. The focused-terminal cycle is
  **1/2 → 2/3 → 1/3 → 1/2**; cycling one terminal does not rewrite another's width.
- Presets refer to the available terminal area, excluding sidebar/header chrome.
  Account for inter-terminal gaps so **1/3 + 1/3 + 1/3**, **1/2 + 1/2** and
  **1/3 + 2/3** fit cleanly. Remove the fixed two-visible-terminal assumption.
  Render whichever columns intersect the viewport, including partial edge columns.
- Each workspace retains its own terminal widths and horizontal view position.
  Switching workspaces, scrolling, changing focus and reordering must preserve
  them. Resizing terminal A cannot make terminal B narrow later merely because
  B occupies the left side. Width preferences persist across close/reopen.
- Mouse resizing previews the focused edge's change and snaps to the same presets
  on release. It changes that terminal's width; neighboring columns retain their
  preferences and move with the strip. Mouse and keyboard reach the same layouts.
  Arbitrary widths are outside the first slice, rather than the default behavior.
- A separate **Toggle terminal full width** action expands the focused terminal
  to the whole terminal area, then restores its previous preset when toggled back.
  Other terminals retain their widths and order. Navigation and Overview remain
  available, and focus/reveal continues through the scrolling strip.
- Width cycling, full width and explicit preset selection appear in Commands and
  Shortcut settings, with discoverable mouse access. Niri's Super bindings are
  inspiration, not Workbench defaults: Niri can consume those keys on Linux.
  Choose editable Workbench bindings consistent with its fresh keyboard model
  and macOS conventions; preserve shell controls and key forwarding.

Niri distinguishes column maximization from native window fullscreen; see its
[fullscreen/maximize reference](https://niri-wm.github.io/niri/Fullscreen-and-Maximize.html)
and [preset widths](https://niri-wm.github.io/niri/Configuration%3A-Layout.html).
Workbench's first action means one terminal uses the available application
terminal area. Native whole-window fullscreen is a separate action/decision.

### State and motion constraints

These widths are client presentation preferences, stored per workspace and
stable terminal ID in each independent window view. They are not shared runtime
facts; two windows can choose different layouts without a catalog mutation or
ownership transfer. Reordering keeps a terminal's width. Moving it between
workspaces in the same window carries its width, leaving destination siblings
unchanged. A previously unseen terminal in another window gets that view's default.
Existing view files need a documented migration; an old global divider must not
become a permanent left/right constraint in the new layout.

Variable widths require cumulative column positions for focus/reveal, hit testing,
visible-range calculation and animation. Preserve immediate typing, interruptible
motion, reduced motion and continuous travel through intermediate terminals.
Apply each intended PTY geometry change once for the committed layout; animating
visual positions must not repeatedly resize live PTYs. Opening Overview continues
to preserve PTY geometry and processes. Keep parsing hidden terminals while
restricting rendering/preview updates to visible columns.

### Review and validation

Exercise the three exact-fit combinations above, eight mixed-width terminals,
first/last jumps, retargeting during resize/navigation, full width/restore, narrow
windows, long names and mixed dark/light themes. Verify workspace isolation,
reorder/move identity, close/reopen persistence and independent window layouts.
Test mouse, trackpad and editable shortcuts on Linux and Mac. Include real terminal
output and live agents; verify typing, selection, scrollback and retained PIDs.
Review motion and visual clarity with the owner and profile 1 and at least 15
populated terminals before accepting the slice. This layout work precedes packaging
and informs the final toolkit decision.

## Reference flows

1. Work in a terminal, change focus, resize panes and navigate a wider workspace.
2. Open the overview, identify an agent needing input and enter its terminal.
3. Drag a thumbnail into another workspace, including an empty workspace;
   reveal offscreen targets, reorder, cancel and handle a full destination.
4. Create/rename a workspace and recover from invalid input or a failed save.
5. Search and preview Ghostty-compatible themes, apply or cancel, and return to
   the same running terminal with the chosen appearance.

Include terminal-dense and sparse workspaces, long names, hidden output, unknown
agent status and disconnected sessions. Evaluate narrow windows and different
pixel densities. Use real terminal content rather than blank placeholder cards.

## Design work

- Establish typography, spacing, corner treatment, surface hierarchy, icon style
  and focused/hover/pressed/disabled states as reusable tokens and components.
- Make workspace position, terminal focus and agent attention easy to understand
  at a glance. Labels and icons must support status colors.
- Give overview entry/exit, scrolling and dragging coherent timing and continuity.
  Motion must be interruptible; input should remain immediate. Reduced motion
  must preserve understandable navigation and feedback.
- Keep gestures predictable across mouse, keyboard and trackpad. Distinguish
  terminal scrolling from workspace navigation, and make alternate actions visible.
- Design terminal and application colors together. Exercise contrasting dark and
  light Ghostty themes, not only one favorable dark palette.
- Review dialogs, empty/loading/error states and theme settings at the same
  standard as the overview. Plan keyboard focus and accessibility semantics.

Transparency and blur are optional experiments. Text readability, platform
support and responsiveness decide whether they belong in the final design.

## Deliverables and acceptance

Deliver the interactive slice, reusable design tokens, screenshots/short recordings
of the reference flows, a recorded review with the owner, and a toolkit decision
with remaining platform limitations stated explicitly.

The review must establish:

- The working view is quiet, legible and recognizably part of the same product as
  the overview, dialogs and theme picker.
- Motion and scrolling feel fluid on a physical desktop, beyond software-rendered
  CI checks. Profile fixed geometry with 1 and at least 15 populated terminals;
  report frame behavior and idle resource use rather than assuming smoothness.
- Theme changes look coherent across terminal content, previews and application
  chrome. Focus, status and destructive actions remain legible in light/dark modes.
- Cancellation, rapid repeated gestures and input during transitions are predictable;
  themes, previews and dragging never restart sessions or resize PTYs. Entering
  the working view may apply its normal terminal layout.
- macOS and Linux results are recorded separately. Unverified native behavior
  keeps the relevant part of the gate open.

Functional checks protect session continuity and input routing. Visual/interaction
review with the owner is required before calling the UX spike complete.

See [the roadmap](roadmap.md) for sequencing and the theme compatibility scope.

## First runnable slice (2026-10-10)

Shared semantic colors now cover workspace chrome, controls, management dialogs,
spatial/list overviews and drag feedback. Rounded surfaces, selected workspace
accents and hover/pressed states establish a first visual vocabulary. Appearance
provides original Night/Dawn palettes and a bounded Ghostty color-file import,
with live preview, Apply/Cancel and independent saved preferences.
See [the supported appearance fields](appearance.md).

Linux/Xvfb checks exercise one and fifteen real terminals at 1240 × 760. They
cover light/dark preview, cancellation, save errors, reopen, imported colors and
malformed imports, along with shell/socket identity, modal input isolation and
unchanged PTY geometry. Screenshots are local artifacts under
`.local/test-results/ux-{1,15}-*.png`, including working view, both overview themes,
appearance, imports and save errors.

The initial appearance slice sampled idle overview CPU for two seconds per theme using process
user/system time (100% means one CPU core):

| Terminals | Dawn | Night |
| --- | --- | --- |
| 1 | 227.3% | 204.9% |
| 15 | 248.5% | 240.0% |

That is +9.3% / +17.1% relative CPU at fifteen terminals in this run. Earlier
runs varied substantially (one-terminal Night reached 238.9%); these short samples
are diagnostic evidence, not a stable benchmark or comparison with the old UI.
Absolute idle use is high under Mesa llvmpipe. Frame-time distributions, physical
GPU/idle measurements and terminal-dense real workloads remain required before
accepting performance or motion quality.

The owner previously confirmed a native macOS build/run. This new appearance
slice still needs Mac testing. Typography, wider catalog compatibility,
smooth workspace navigation, accessibility and native input remain open. Owner
visual/interaction review and the final toolkit decision are pending; this slice
does not complete the UX gate.


The appearance slice now has a TerminalColors-inspired offline card gallery with
name search and All/Dark/Light filters. Dracula and four Catppuccin variants are
bundled from pinned official repositories with MIT notices preserved. Every card
renders a terminal sample and ANSI swatches in that palette. Search/no-match,
filtering, dark/light apply/reopen and narrow-window scrolling are covered by the
one/fifteen-terminal GUI check. Local screenshots add `gallery-*` artifacts.
The source files pass through the same reader as imported Ghostty color files.
This extends the runnable slice; native visual review and the toolkit gate stay open.


Scoped appearance now separates application colors, workspace themes, terminal
overrides and default terminal colors. Stable chrome is the default; follow-active-
workspace mode offers matching chrome. A terminal override carries through moves,
while an inheriting terminal adopts its destination's workspace colors. Mixed-color
overview previews and a single Apply/Cancel transaction exercise both directions
for owner review. Existing global preferences migrate without modifying session
metadata; malformed scoped preferences remain untouched.


Scoped appearance validation passed on Linux/Xvfb with one and fifteen live
terminals. It covers mixed previews, stable/follow chrome, whole-dialog Cancel,
inheritance reset, scope persistence/reopen, moved explicit and inheriting terminals,
real light/dark color-scheme query responses, legacy migration and malformed-state
preservation. The terminal/workspace/management/move and animated/reduced-motion
overview GUI regressions also pass. Mac review remains open.

The scoped run's two-second idle overview samples were 209.5% / 228.0% CPU for
one terminal (Dawn / Night chrome), and 239.5% / 240.5% for fifteen: relative
increases of 14.3% / 5.5%. These software-rendered samples have the same short-run
variance and high absolute idle use described above; they do not establish native
frame smoothness. No final toolkit or UX acceptance decision is implied.

## Working-view navigation slice

The header now combines Prev/Next, a compact focused-position count and clickable
numbered positions. The focused position uses an accent mark; visible
positions use stronger surfaces/text than offscreen positions. Hover shows terminal
names or the scrolling affordance. The working area stays the same size.

Header-only trackpad/mouse-wheel navigation accumulates fractional deltas and
bounds momentum; terminal scrolling keeps its existing routing. Revealing panes
uses a continuous pane strip: 270ms for one position plus 180ms per additional
position, capped at 1.25s. An eight-to-one jump visibly crosses the intermediate
panes rather than swapping source/destination pairs. Retargeting begins at the
current fractional position. Only intersecting tiles perform presentation work;
tiles render on entry once per navigation and retain their terminal backgrounds
and titles. Up to eight reusable tile textures are retained for the active strip;
steady frames render only visible pane canvases. Half-width panes show two full
columns; thirds show three, with partial columns allowed at the viewport edges. No background workspace is rendered for navigation.
Target layout and focus commit normally; intermediate presentation never resizes
PTYs. Column positions and clipping now follow each terminal's preferred width,
without special left/right destination slots.
Reduced motion, window resize, modals and terminal pointer interaction settle it.

Linux/Xvfb checks use populated one- and fifteen-terminal fixtures at 1240×760.
They cover position selection, rapid retargeting, immediate typing, header-only
vertical/horizontal wheel input, terminal scroll preservation, transition-time
pointer routing, boundaries, independent snapped widths, narrow/empty workspaces,
overview handoff and retained
PID/socket identity. Once terminals have visited their normal layout, sampled PTY
geometry remains unchanged throughout navigation. Pixel checks cover start, an
intermediate scroll and completion. A live X11 pixel sampler checks that distinct
backgrounds from every intermediate pane appear during an eight-to-one jump.
Screenshots under `.local/test-results/` use
`nav-15-animated-{start,moving,settled,jump-moving,wide,narrow,narrow-divider}.png`;
reduced motion has a
separate prefix.

The final sequential two-second idle samples were 216.0% CPU with one terminal
and 213.9% with fifteen (-1.0%); fifteen in reduced motion sampled 224.3%. These
llvmpipe samples remain noisy and consume roughly two CPU cores. They demonstrate
no reliable performance improvement, and do not establish physical-GPU frame
smoothness. The owner confirmed that continuous pane navigation works on Mac,
including an eight-to-one jump with alternating dark/light terminal themes.
Full native input/UX acceptance, physical Linux/Wayland review, frame-time
distributions and the final toolkit decision remain open.

### Overview scrolling

Overview scrolling now retains fractional two-axis wheel/trackpad deltas instead
of turning any nonzero delta into one workspace or three list rows. A unit of
GLFW wheel input moves 40 presentation pixels; large per-frame bursts are capped
at 120 pixels. Short, time-based easing follows this target with immediate
reversal and clamped boundaries. The OS-provided momentum is consumed as input;
Workbench adds no extra fling. Reduced motion uses the same fractional targets
without easing. Horizontal swipes and Shift+vertical scroll move the workspace
under the pointer; ordinary vertical gestures scroll workspace rows or the list.

Partial cards/list rows are clipped to the overview content area. Drawing,
hit-testing, drag destinations and live-preview visibility use the same position;
previews keep their existing cached refresh and never resize the PTY. Native
Mac trackpad speed/feel still needs owner acceptance. Linux checks cover the
native callback's accumulation/reset, pure fractional scrolling/reversal/bounds,
normal and reduced-motion overview navigation, and drag scrolling. The populated
GUI fixture can run with eight workspaces and eight terminals per workspace.

Empty workspace rows keep the overview's background rather than drawing a separate
filled or bordered container. Their label and drop hint remain visible; an active
drag still supplies its insertion/drop feedback.

For terminals owned by another window, Move here lives in the terminal header
alongside Stop and Rename. Its explanation uses an opaque footer; neither the
action nor its text floats over the retained terminal screen. Drawing and hit
testing share the same button rectangle, including during pane navigation.

## Independent-width prototype

The prototype now cycles half → two thirds → third for the focused terminal,
selects explicit presets through Commands, and toggles full terminal-area width
without losing its preferred preset. Linux defaults are Ctrl+Shift+R/F; Mac
uses Command+Option+R/F. Shortcut settings can edit or clear these bindings.
A right-gap drag displays a snapped guide and commits on release. Other terminal
preferences stay unchanged. A lone terminal starts at half width too.

Version 2 window views persist width/full-width preferences by stable terminal ID
and normalized working-view positions per workspace. Version 1 restores existing
focus and overview while retiring its positional divider. See [view migration](window-state.md#independent-terminal-widths).
Hidden workspaces retain runtime geometry; entering them applies their preferred
visible geometry. Window resizing rescales their client scroll coordinates without
resizing hidden PTYs. Widths stay with live pane objects on in-window moves.

Pure checks cover exact half/half, third/third/third and third/two-thirds fits,
mixed eight-column reveal and continuous visibility, snap thresholds, full width,
empty strips, transactional view migration, width record validation, and old
shortcut overrides that conflict with newly introduced defaults. The live
navigation harness covers shortcut/palette cycling, full width/restore, three
thirds, a two-thirds/third pair, content clicks in the third terminal, snapped
resize-on-release, unchanged siblings, workspace isolation, and retained shell
PID/socket state. It also exercises ordinary and reduced-motion navigation.
`just check` and the Linux viewer build pass. Window-view checks cover width
restoration across reorder/reopen and independent window preferences; Commands,
management/recovery, bidirectional ownership transfer, workspace moves and animated
overview checks pass with live shells. Native acceptance remains separate.
Screenshots use `nav-15-{animated,reduced}-{three-thirds,two-thirds-and-third}.png`
under `.local/test-results/`.

Completed sequential 1240×760 Linux/Xvfb/llvmpipe navigation runs sampled two-second
idle CPU at 79.0% with one terminal and 78.5% with fifteen (-0.6%). One terminal
renders its half-width column; the fifteen-terminal fixture has eight in the
active workspace and seven hidden, with two half-width columns visible at rest.
These short software-rendered samples are noisy and show no reliable scaling
regression; they do not establish native frame pacing or GPU performance.
Native Linux/Wayland and Mac width gestures, editable shortcuts, visual motion,
and mixed-theme agent layouts still require owner review. The UX gate remains open.

## Shortcut-help prototype

The grouped help sheet now shows the live configured action map, including unbound
actions, and reuses application colors and rounded panels. At 1240×760 every group
fits in two columns; narrow windows use a vertically scrollable single column.
Escape/Done/toggle return to the underlying view, and Enter/Edit shortcuts opens
settings. Help works over Overview while preserving its query and selection.
Mac application defaults share Command+Option (P/O/T/N, navigation, widths and /);
copy/paste retain Command+C/V. Saved overrides are preserved.

Bindings reload only on opening. A single retained body texture redraws for open,
scroll or size changes; modal buttons keep live hover states. If texture allocation
fails, the same content draws directly. Help consumes input, preserves PTY geometry
even during overlay window resize, and continues parsing runtime output. Its state
is temporary and does not become a server/session fact or a restored window mode.

`just check`, the Linux viewer build, Commands and Overview regression checks pass.
`just shortcuts-gui-test` passes ordinary/reduced-motion runs with live shells:
slash/Shift input isolation, current/custom/unbound maps, settings/recorder handoff,
Overview layering, dialog precedence, narrow cached-body redraw and light/dark
colors, with retained shell PIDs/state/geometry. Captures are
`.local/test-results/shortcut-help-{wide,custom,small,small-home,light}.png`.

`just shortcuts-scale-gui-test` uses fixed 1240×760 windows and populated terminals.
One terminal fills the estate; fifteen have eight active-workspace terminals and
seven hidden, with two half-width columns visible. The final sequential two-second
idle samples were 249.9% CPU with one terminal and 272.0% with fifteen (+8.8%;
100% is one CPU core). Earlier direct-body drawing sampled 262.9% / 289.7%. These
short llvmpipe samples are noisy and do not establish a reliable performance gain
or native frame pacing. The live check also verifies one body texture allocation
through idle frames and retained terminal geometry/processes.

Native Mac slash/Option behavior, non-US layouts, physical trackpad gestures,
accessibility and owner review of the sheet remain open. This completes the runnable
help slice's Linux functional proof, not the wider UX/toolkit gate.

## Keyboard focus prototype

Tab/Shift+Tab now traverses management dialogs, Commands, help/settings/recording,
Overview and Appearance. A two-pixel accent outline marks keyboard focus without
adding permanent chrome. Enter/Space activates focused buttons. Text, paste and
select-all reach only the focused field; search/grid Enter retains the existing
quick action. Shortcut capture has a separate field, so traversing or activating
its buttons does not record those keys.

Stop and recovery start with no confirmation selected. Initial Enter/Space cannot
terminate or replace a shell; Tab first selects Cancel, then the confirmation.
Escape returns to the underlying view/focus. Help preserves Overview's focused
filter as well as its query/selection. Appearance cancellation restores the whole
transaction. Busy management disables activation and retains field focus for
correction after an error. Disabled Appearance controls are skipped.

The full `just check` suite, Linux viewer build and Commands/management/Appearance
GUI regressions pass. Appearance was exercised with 1 and 15 populated panes.
Help and Overview pass ordinary/reduced-motion regressions. `just focus-gui-test`
checks forward/reverse traversal, field/button input isolation, workspace folder
creation, settings/recorder cancellation, layered focus restoration, appearance
scopes/follow/apply/cancel, explicit stop and fresh-shell recovery, narrow help,
and retained live-shell state. Dark/light captures are under
`.local/test-results/focus-*.png` and were visually reviewed.

Traversal checks at most 32 client controls per Tab press. Focus drawing adds
only a ring for the selected visible control; no filesystem/process queries or
runtime-state aggregation occur in pane rendering. Appearance reuses its existing
match array and rescans only after query/filter changes, avoiding an added full
catalog scan on every frame. Help's retained body texture stays reusable while
button focus changes.

`just shortcuts-scale-gui-test` keeps the Edit shortcuts focus outline visible
at fixed 1240×760 geometry. Sequential two-second llvmpipe samples measured
242.3% CPU with one populated terminal and 258.3% with fifteen (+6.6%; 100% is
one core). Both checks retained PID/socket/geometry and one help-body allocation.
These short samples do not establish native frame pacing or a performance gain.

Native Mac focus, Option/slash handling, non-US layouts, IME, physical trackpad
interaction, Retina and accessibility acceptance remain open. The UI gate is
not yet complete. See [Mac focus checks](macos-build.md#keyboard-focus-acceptance).

## Typography and surface polish prototype

The owner confirmed basic Mac keyboard focus, then requested stronger typography
and general polish, also reporting soft Mac terminal text. The current slice
adds bundled Adwaita Sans for interface text and keeps terminal content and
keycaps monospace. Shared 12/14/16/22/24-pixel text roles, larger field/title text,
consistent corners, quieter disabled controls and explicit action/hover/pressed
outlines improve hierarchy without adding permanent chrome.

Source inspection found Retina atlases being reduced into logical-size pane
textures and enlarged for display. Pane and navigation canvases now allocate at
backing resolution, draw logical coordinates through a camera and snap presentation
to backing pixels. Help's retained body follows the same resolution and clip
contract. Terminal sizing and input coordinates remain logical.

See [the rendering and typography details](interface-typography.md). Native Mac
font appearance, Retina/non-Retina and monitor transitions need owner review;
Linux simulated 2x checks cannot substitute for that. The overall UI gate stays
open for visual acceptance, complex shaping, IME and accessibility.

Validation for this slice passes `just check`, the Linux viewer build, help and
Overview (ordinary/reduced motion), Commands, Appearance (1/15 populated panes),
Unicode glyph rendering and `just retina-gui-test`. The latter protects the
existing 7×16 grid: 1x and 2x both reported 41 rows / 72 columns in the fixed
half-pane fixture, retained live shell state, exercised explicit stop/recovery,
and verified 1026×1416 pane and 1840×1156 help backing allocations at 2x.
Doubled-resolution navigation also passes with 15 populated terminals.

Dark/light captures were visually reviewed, including help, fields, action/focus
outlines and Appearance. Captures live under `.local/test-results/`, including
`focus-help-edit.png`, `focus-appearance-cancel-light.png` and `retina-focus-*.png`.

Fixed 1240×760, sequential two-second help samples with a visible focus outline
measured 250.9% / 278.4% CPU for 1 / 15 terminals at 1x (+11.0%), and
286.5% / 333.4% at 2x (+16.4%; 100% is one CPU core). Every run retained
PID/socket/geometry and one help-body allocation through idle. These short
llvmpipe measurements are noisy; they establish neither a performance gain nor
native GPU/frame-pacing acceptance. Backing textures cost four times the pixels
at 2x, while fixed overview previews stay bounded. Interface font atlases load
once per window; the new draw helpers do not add filesystem/process inspection
or terminal-state aggregation to pane loops.

## Pane-header and sidebar refinement

The owner reported the typography/sharpness pass was much better and requested
continued chrome polish. This slice adds clearer active workspace/pane markers,
quiet secondary actions, workspace saved-terminal counts, separate agent status
pills and bounded UTF-8 ellipses. It retains terminal geometry, action targets,
input routing and all navigation shortcuts. Details and review steps are in
[interface typography](interface-typography.md#pane-headers-and-workspace-sidebar).

The Linux build and `just check` pass. Management regressions cover creation,
rename, capacity, empty states, agent lifecycle/attention and explicit stop/recovery.
Multiwindow regressions retain bidirectional ownership transfer and same shell
PID/state/socket. `just retina-gui-test` keeps the fixed 7×16 grid at 1x/2x and
exercises 15 populated terminals at doubled backing resolution. Normal/reduced
motion navigation also passes.

Disposable review captures cover dark/light chrome, long Unicode names, focus
changes and the minimum window: `.local/test-results/chrome-{dark,light,small}.png`.
Working, Needs input and Ready header captures use an isolated named-process
fixture with real runtime hook reports, not a real coding CLI. These and the
narrow agent header are `.local/test-results/chrome-agent-*.png`.
Native Mac and owner visual acceptance of the new chrome remain open.

The final fixed 1240×760 help/working-view samples measured 252.9% CPU with one
populated terminal and 277.0% with fifteen (+9.5%; 100% is one core). The same
fixture before this refinement sampled 250.9% / 278.4%. Each two-second
llvmpipe run retained PID/socket/geometry and one help-body allocation through
idle. These short samples show no clear scaling change and do not establish
native frame pacing. Label fitting adds bounded work per visible row/header,
not per PTY byte or hidden terminal output, and introduces no heap allocation
or runtime inspection in those loops.

## Minimal chrome and navigation study

The owner supplied Niri, Dank Material and Foot references and clarified that
the proposed top-bar position indicators should represent panes. Workspace and
agent navigation still belongs in the left-side design, without assuming
Herdr's combined list is the final answer.

[The design proposal](minimal-chrome.md) and
[interactive sketch](prototypes/minimal-chrome.html) compare a compact rail with
a pinned named panel. Separate Workspaces/Agents modes retain cross-workspace
agent attention while removing permanent secondary actions from the working
view. Top-bar pane indicators reflect focus and viewport intersection; pane
headers keep names and activity plus a visible overflow affordance.

This slice is sample-data HTML for structural review. It does not change native
viewer behavior or establish platform/runtime acceptance. Review the placement,
discoverability, agent context and compact/pinned tradeoff before implementing
shared menus, indicator geometry and sidebar layout in the app. Preserve existing
keyboard navigation, scoped themes, explicit stop/recovery and multiwindow facts.
