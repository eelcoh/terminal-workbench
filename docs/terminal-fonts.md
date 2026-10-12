# Terminal Unicode fonts

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
Terminal text retains the existing monospace font and 7×16 logical grid. Pane
and navigation canvases now render at backing-pixel resolution, removing the
logical-texture reduction/enlargement stage on Retina displays. Help uses a
matching backing texture. See [typography and backing resolution](interface-typography.md).

The owner reported soft terminal text on Mac before this change. Native Mac
sharpness of the corrected pipeline remains to be reviewed after rebuilding.
