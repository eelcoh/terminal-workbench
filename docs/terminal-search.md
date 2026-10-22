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
matches, with a visible limit/refine-query hint. The Qt options below add simple
case folding and word boundaries. Regex, full case-fold expansions and Unicode
normalization remain follow-ups; default queries match literal codepoint/byte
sequences.

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

## Qt workbench

The Qt candidate provides literal search through **Find in terminal**
in Commands, the application menu and the inherited Cmd–Option–S / Super–Alt–S
binding. Existing `terminal.search` dotfile and UI shortcut overrides apply; the
shortcut editor and help show the active binding. In Overview this chord instead focuses Find terminals, which searches workspace,
folder, Git and agent metadata. Terminal-output search waits until attachment
finishes.

The compact bar appears over the selected terminal and reveals it if it was
scrolled offscreen. It uses the existing themed controls and a native editable
field without dimming the terminal. Enter/Down advances, Shift+Enter/Up reverses,
and Tab reaches the previous/next/close buttons. Buttons accept Enter and Space.
Matches are highlighted in the terminal; the initial selection is the last match.
Escape, the close button or an outside click restores the starting viewport and
returns focus. Query text is temporary and is excluded from saved window views
and ordinary display reports.

The existing bounded C scanner is shared with the prototype through a small
adapter. A lazy Qt controller owns debounce/batch timers, the tracked viewport
bookmark and precomputed highlight spans. Only an open search scans; VT writes
and reflow immediately invalidate results before the next batch. Painting reads
cached spans and never queries terminal history. Closing releases highlight-mask
memory. Retained text in a stopped/disconnected local replica remains searchable;
this cannot reconnect, take ownership or start a replacement shell. Its lifecycle
controls return when search closes. Terminal replacement dismisses search safely.

`just toolkit-search-test` checks the Qt controller against real Ghostty state,
including tracked/live viewport restoration, wrapping, Unicode cell mappings,
soft/hard line boundaries, reflow, alternate screens, mutation invalidation,
UTF-8 query/result bounds and cancellation. `just toolkit-shortcuts-test` checks
Mac/Linux default modifiers and managed search overrides. `just toolkit-search-gui-test` runs the focused
search workflow on both native Linux backends; `just toolkit-ui-test`
checks native X11/Wayland query focus, result navigation, history reveal, Commands
handoff, live-output refresh, stopped-terminal search without recovery, viewport
restoration, and unchanged PTY bytes/readers/grids. Captures are under
`.local/artifacts/qt-ui/{xcb,wayland}/terminal-search*.png`. Native Mac visual,
keyboard and clipboard acceptance remains pending.


## Qt case and word options

Find in terminal now includes **Match case** and **Whole word** buttons. Match
case starts enabled and Whole word starts disabled, preserving literal substring
search. Press either button to toggle it; the query retains focus and matches
rescan. Tab reaches both options after the existing previous/next/close controls;
Enter and Space activate a focused button. An active search keeps ordinary
application navigation and terminal input scoped away from the query field.
Enter/Up/Down navigation defers while the field is composing text.

With Match case disabled, Unicode simple case folding handles accented letters,
Greek sigma variants, Kelvin-sign/K and non-BMP case pairs. Folding preserves
one source codepoint per transformed codepoint, so highlights still refer to
original terminal cells even when UTF-8 byte lengths differ. It does not perform
multi-codepoint expansion (`ß` and `ss` stay distinct), locale-specific casing or
Unicode normalization. Composed and decomposed accents remain literal sequences.
Both original and folded query bytes must fit the existing 255-byte bound;
overflow produces an inline query error rather than an incomplete search.

Whole word requires the characters immediately before and after a match to be
outside Unicode letters, numbers, combining marks and underscore. This prevents
`needle` matching `needles`, `_needle`, `needle_` or an accented continuation.
It is a character-boundary rule, rather than language-specific dictionary word
segmentation. Soft-wrapped rows stay one logical line; hard newlines and the end
of retained text finish a word. A pending right-boundary decision survives scan
batch boundaries without prematurely highlighting a partial word.

Options belong to each pane's client search controller. They remain in memory
when its search closes, and are not saved to dotfiles or window restoration.
Other panes and windows keep their own options. Changes, output and reflow clear
stale results immediately and rescan through the existing debounce and bounded
batches. The 1,024-result cap, tracked viewport restoration and closed-search
behavior remain unchanged. Case conversion and word classification add no
per-cell allocations, whole-buffer formatting or runtime requests.

`just toolkit-search-test` covers the options against real Ghostty text,
including Unicode folds, combining cells, query bounds and reflow. `just check`
retains prototype literal behavior and exercises word decisions across one-cell
VT batches. `just toolkit-search-gui-test` checks both Linux display backends:
option buttons, keyboard navigation, live-output invalidation, Unicode queries,
unchanged reader identities/grids and no query leakage into PTY input.
Native Mac keyboard, composition and visual review remain required.
