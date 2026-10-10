# UX spike brief

## Goal

Approach the beauty and feel of [Dank Linux / DankMaterialShell](https://danklinux.com/)
while retaining Foot-inspired simplicity and a terminal-first working view.
This is a quality requirement from the owner. The existing prototype proves
interactions; its current appearance is a starting point.

The spike produces a runnable slice and informs the final UI toolkit decision.
No toolkit is selected solely because it can reproduce a static screenshot.

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
numbered positions. The focused position uses an accent mark; the two visible
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
steady frames keep the normal two-canvas path. At the standard equal divider,
only two or three tiles intersect the working area; asymmetric layouts can show
more. No background workspace is rendered for navigation.
Target layout and focus commit normally; intermediate presentation never resizes
PTYs. The right destination expands/contracts into an asymmetric divider slot
as it arrives, covering subsequent strip tiles at rest.
Reduced motion, window resize, modals and terminal pointer interaction settle it.

Linux/Xvfb checks use populated one- and fifteen-terminal fixtures at 1240×760.
They cover position selection, rapid retargeting, immediate typing, header-only
vertical/horizontal wheel input, terminal scroll preservation, transition-time
pointer routing, boundaries, asymmetric dividers, narrow/empty workspaces,
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
smoothness. Native Mac trackpad/visual acceptance, physical Linux/Wayland review,
frame-time distributions and the final toolkit decision remain open.
