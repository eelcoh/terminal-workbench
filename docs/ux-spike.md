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
