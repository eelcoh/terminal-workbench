# Terminal fonts and sizing

The Ghostling spike previously loaded Raylib's default ASCII glyph set from its
bundled JetBrains Mono font. That replaced Unicode block art, arrows and other
symbols with question marks even though terminal text and clipboard bytes were
correct.

The viewer now loads the actual Unicode mappings from the pinned JetBrains Mono
font and adds unmodified Noto Sans Symbols 2 Regular 2.008 for characters missing
from the primary font. This covers Claude's logo and prompt/status symbols,
including `▐▛███▛█`, `▝▜██████▀`, `←`, `❯`, `⏵`, `◐`, `✻` and the package icon `📦`.
Both fonts are embedded; neither platform needs a system font installation.
The Noto font is monochrome. Its license, digest and source are recorded under
[fonts/noto-symbols](../fonts/noto-symbols/README.md).

Font atlases are loaded once per window. Terminal glyph lookup keeps a direct
ASCII path and binary search for Unicode, avoiding Raylib's linear scan through
the expanded glyph set for every terminal cell. Existing cell geometry, PTY
resizing, selection and clipboard text are preserved. Both the working view and
overview previews use this renderer. Unsupported glyphs still fall back to `?`.
This does not establish complete Unicode coverage, complex-script shaping or
color-emoji/emoji-sequence support; those remain toolkit acceptance requirements.

`just check` validates cmap formats 4/12, missing-glyph exclusion, supplementary
characters, required fallback symbols, the bundled digest and OFL license.
`just font-gui-test` renders a terminal specimen in Xvfb and compares each glyph
with a literal question-mark baseline, also requiring visible ink. The retained
image is `.local/test-results/fonts.png`. The owner confirmed glyph rendering
on Linux and native macOS in GitHub Copilot CLI. Specific Retina scale coverage
and the broader shaping requirements remain open.
Running terminals can stay alive during the viewer rebuild.

## Terminal sharpness and interface fonts

Interface chrome now has its own bundled sans-serif face and type hierarchy.
The default terminal text uses JetBrains Mono with a 7×16 logical grid. Pane
and navigation canvases now render at backing-pixel resolution, removing the
logical-texture reduction/enlargement stage on Retina displays. Help uses a
matching backing texture. See [typography and backing resolution](interface-typography.md).

The owner reported soft terminal text on Mac before this change. Native Mac
sharpness of the corrected pipeline remains to be reviewed after rebuilding.

## Choosing a font and size

Open **Terminal text settings** from the hamburger menu or Commands. Choose
**JetBrains Mono** or **Fira Code** and a size from **10 to 32 pt**. The terminal
and sample preview immediately; **Apply** keeps the choice for this window,
**Cancel** / Escape restores its previous settings, and **Use defaults** returns
both fields to your [dotfile defaults](dotfiles.md#terminal-fonts).

| Action | Linux | macOS |
| --- | --- | --- |
| Increase terminal text size | Ctrl+Shift+= | Command+Option+= |
| Decrease terminal text size | Ctrl+Shift+- | Command+Option+- |
| Reset terminal text size | Ctrl+Shift+0 | Command+Option+0 |

These use the Equal, Minus and 0 keys and are editable in Shortcut settings.
They apply in the working view; dialogs, help and Overview retain their input
ownership. Reset restores configured size while retaining your chosen face.
Interface typography stays independent. Logical zoom resizes the visible
terminal grid, preserving processes, scrollback and terminal state; hidden
workspaces receive their new grid when entered. Display density refreshes the
selected face's atlas without changing logical cells or resizing the grid.

Fira Code Regular 6.002 is embedded with its upstream OFL license, source and
digest in [fonts/fira-code](../fonts/fira-code/README.md). The renderer uses its
normal glyphs; programming ligatures, complex shaping and installed-font browsing
are follow-up work.

`just font-settings-gui-test` exercises native font/size changes with one and
fifteen live terminals, window restore, platform defaults, density transitions,
selection/clipboard, variable-width navigation, modal input and shell continuity.
The full check suite, chrome, display and dotfile GUI regressions pass on Linux.
Native macOS keyboard/visual review remains
pending. Screenshots are retained under `.local/test-results/font-settings-*`.

## Native Mac rasterization and external displays

The owner reviewed a native Ghostty comparison on an LG 35WN75C at its default
3440×1440 resolution. Pane borders look correct there; terminal text still looked
softer than Ghostty. This does not complete review of the earlier external screen
or unplug recovery.

Mac terminal atlases now use CoreText grayscale glyph coverage at the current
backing density, without synthetic font thickening. Bundled JetBrains Mono,
Fira Code and the symbol fallback share this path. The existing rasterizer still
provides logical advances/cell measurements and individual fallback glyphs if a
native glyph cannot be drawn. Atlas packing accounts for padded native ink bounds.
Linux retains its current rasterizer. No font/config migration or extra installed
dependency is needed; CoreText/CoreGraphics are system frameworks.

After rebuilding with `just spike`, the viewer log records `Terminal glyph
rasterizer: CoreText N/M ...`. Compare the same font and size against Ghostty on
the 1× external screen, then move to the MacBook display and back. Review sharpness,
upright glyphs/bearings, Unicode symbols, selection alignment and unchanged grid
on density-only moves. Native Mac build and visual acceptance remain pending.
`just check` covers metric parsing, ink bounds, surrogate pairs and adapter
fallback/resource cleanup using API stand-ins; these do not validate Apple's SDK
or the resulting pixels on Mac hardware.

The Linux spike build and `just check` pass. Font-settings and display GUI
regressions pass with one and fifteen terminals, including simulated 1→2→1
density, selection/clipboard, borders, resizing and retained shell/grid state.
