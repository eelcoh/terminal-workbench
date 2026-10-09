# First integration spike

## Goal

One graphical terminal in a window on macOS and Linux. Prove the integration
before committing to a UI toolkit or importing runtime implementation.

## Sequence

1. Examine Herdr's runtime API and Ghostty's full embedding surface. Record
   process ownership, transport options, licensing, and API stability concerns.
2. Select and pin an integration revision. Host one terminal on each platform,
   using the same terminal foundation.
3. Exercise a shell and a full-screen application. Validate text input, control
   keys, resize, selection, clipboard, and scrollback.
4. Prove runtime-owned process lifetime: detach the window, reattach, and confirm
   the same running process and coherent terminal contents. If the embedding API
   prevents this, document the adaptation and resolve it before proceeding.
5. Record build/run commands and platform results, then choose the UI approach.

## Acceptance checks

| Behavior | macOS | Linux |
| --- | --- | --- |
| Build and open a terminal window | Pending | Passed, X11/Xvfb |
| Shell input and terminal control keys | Pending | Passed, basic typing/Ctrl-C/Escape |
| Full-screen application and resizing | Pending | Passed, vi edit/save and PTY resize |
| Unicode, composition, and keyboard layouts | Pending | Pending |
| Selection, clipboard, and scrollback | Pending | Passed, X11/Xvfb selection, clipboard transfer, wheel history |
| Detach and reconnect to the same live process | Pending | Passed, within replay limit |

Linux validation cannot establish macOS renderer or native input behavior. Run
the macOS checks on a Mac before marking this milestone complete.

## Following milestone

A Linux layout experiment now has a workspace sidebar, two horizontally arranged
terminal panes per workspace, and unseen-output indicators. Focused input,
selection/clipboard ownership, independent resize, hidden output, and reconnect
are covered by `just workspace-gui-test`. `just management-gui-test`
also covers workspace/terminal creation and renaming, cancellation, invalid names,
third-terminal navigation, confirmed close/empty states, saved labels, retained shell
identities on reopen, and a searchable overview with direct terminal jumps. Agent attention
detection, the scrolling layout, and new navigation shortcuts follow this prototype.

The spike is runnable on Linux. The milestone remains open until the pending
platform and interaction checks are complete. See [integration findings](integration-findings.md).
