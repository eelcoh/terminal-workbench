# Find in terminal

Choose **Find in terminal** in Commands, or press **Super+Alt+S** on Linux /
**Command+Option+S** on Mac. These defaults are configurable through Shortcut
settings and appear in shortcut help. The existing full-width F shortcut stays
available. Older custom S bindings take precedence over the new inherited default.

The compact bar searches literal, case-sensitive text in the focused terminal's
retained screen and scrollback. Soft-wrapped text is searched as a continuous
line; a hard newline separates matches. UTF-8 queries include wide and combining
characters, with highlights mapped back to terminal cells. Terminal glyph coverage
remains governed by the existing fonts; search does not add shaping or glyphs.

- Type or paste a query; highlights and a selected/total counter appear after scanning.
- Enter / Down selects the next match; Shift+Enter / Up selects the previous one.
  Navigation wraps. The initial selection is the last match in the result set.
- Use the arrow buttons with the mouse, or Tab / Shift+Tab and Enter / Space.
- The field supports caret movement, selection, Home/End and desktop Select all /
  copy / cut / paste. Text cannot reach the terminal while search owns input.
- Escape, ×, or an outside click closes search and restores its starting viewport.
  A starting live-output view returns to live output; a retained-history bookmark
  follows ordinary output/reflow where Ghostty can preserve it.

Search is window-local and temporary. It never takes ownership of another window's
terminal, changes the PTY grid, sends the query to the shell or stores query text
in dotfiles/window restoration. Existing selection remains independent. Query
text is not included in diagnostics; match counts and viewport positions are.
The active alternate screen is searched independently of primary-screen history.
Stopped panes can search their retained local text; connecting/replaying panes
must finish attaching first. It is not cross-terminal search or a server-wide index.

## Work bounds and output changes

Queries are single-line UTF-8, at most 255 bytes. Results retain the first 1024
matches, with a visible limit/refine-query hint. Regex, case folding and Unicode
normalization are follow-ups; exact codepoint/byte sequences match literally.

Only the selected pane is scanned, outside rendering, in bounded batches with a
4096-cell / approximately 2ms per-frame budget. There is no whole-buffer formatting,
new runtime request or worker reading terminal state concurrently with its parser.
The current pinned API resolves a grid reference once per row/batch and reads
individual cells from that row. Memory is bounded by the query/result caps and
one viewport highlight mask (at most 1000×1000 cells), acquired outside rendering.
Hidden/background panes retain their existing parsing/presentation behavior.

Typing, new output and resize/reflow invalidate stale matches/highlights. A scan
starts after a 120ms pause and continues incrementally; continuously changing
output can delay completed results. Shells continue running and output continues
parsing throughout. A terminal replacement or changed search target dismisses
search safely, and invalidated history bookmarks are not used as grid references.

## Validation

`just check` covers overlapping matches, literal case behavior, Unicode mappings,
query/result bounds, shortcut compatibility, and the actual pinned Ghostty API's
scrollback, soft wraps, hard newlines, wide/combining cells, alternate screens,
resize/reflow and read-only viewport behavior. The Linux viewer build passes.

`just search-gui-test` checks real windows with one and fifteen terminals: visible
highlights, next/previous wrap, no-match/input isolation, field clipboard edits,
Commands handoff, new-output refresh, Escape restoration, and unchanged shell
PID/socket/grid. It also passes at simulated doubled backing density. Captures
are under `.local/artifacts/search/`. F12 screenshot echo is excluded from the
reading-area comparison and cleared before subsequent fixture shell commands.
Native Mac keyboard/visual acceptance remains pending; Xvfb does not prove it.
