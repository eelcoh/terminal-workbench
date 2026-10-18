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
| Increase terminal text size | Super+Alt+= | Command+Option+= |
| Decrease terminal text size | Super+Alt+- | Command+Option+- |
| Reset terminal text size | Super+Alt+0 | Command+Option+0 |

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

## External-display font review

The owner reviewed a native Ghostty comparison on an LG 35WN75C at its default
3440×1440 resolution. Pane borders look correct there; terminal text still looked
softer than Ghostty. This does not complete review of the earlier external screen
or unplug recovery.

The subsequent CoreText terminal-atlas experiment built and loaded on Mac, but
the owner reported no improvement and possibly worse text. It has been reverted.
Terminal text again uses Raylib's original stb rasterizer on both platforms.
The confirmed Mac application/menu-name fix remains in place. Bundled fonts,
font-size preferences, logical cell geometry and backing-density handling retain
their existing behavior.

External-screen sharpness remains an open UX gate. The next investigation needs
the same font, actual glyph size, colors and geometry beside native Ghostty, with
pixel-level evidence of any rasterization or texture-sampling difference before
another renderer change is adopted. A successful build/atlas upload is not visual
acceptance; Linux display checks do not prove native Mac appearance.

Source inspection also shows a size-comparison trap: Workbench currently uses
stb pixel-height scaling and the selected size as cell height, while the pinned
Ghostty CoreText face uses point/em sizing. Equal numbers in the two applications
do not establish equal glyph size. Match the rendered glyph footprint before
judging edge sharpness; changing Workbench's sizing semantics requires a separate
grid/preferences migration decision.

Rollback validation: the Linux spike build, `just check` (including Mac identity
metadata wiring), and the primary/fallback Unicode font GUI check pass.


## Qt candidate: terminal text and dotfile defaults

Run `just toolkit-ui`, then choose **Terminal text…** from the hamburger menu or
Commands. Select **JetBrains Mono** or **Fira Code**, adjust the size from **10 to
32 logical points**, and preview it across the window. **Apply** keeps the choice
for this study window, including terminals created afterward; **Cancel** or Escape
restores the opening font/size. **Use dotfile defaults** previews the configured
face and size, and can itself be cancelled.

Use Command+Option on macOS / Super+Alt on Linux with **=**, **−** or **0** to
increase, decrease or reset size. Reset retains the selected face. These actions
also appear in Commands and shortcut help. Dialogs own input before these bindings;
interface text retains its independent bundled Adwaita Sans font.

The launcher uses the existing config directory, including `WORKBENCH_CONFIG_DIR`,
and reads `config.conf` once. Shared `[font]` and `[macos.font]` / `[linux.font]`
resolve through the same versioned parser as `just spike`. Platform fields override
shared fields regardless of order. Missing files use JetBrains Mono at 16 pt;
invalid files fall back transactionally with a dismissible startup diagnostic.
The managed file and its symlink/comments stay untouched. The complete existing
config is validated, but Qt currently consumes only font fields from it; managed
appearance/shortcuts and per-window saved font overrides remain follow-ups.

Both faces are bundled with their OFL notices. The existing shaped row renderer
uses the chosen face and Qt point metrics, with backing density handled separately.
Font changes resize existing PTYs and invalidate affected presentation caches;
they do not reconnect, replace or transfer a shell. Retained hidden panes update
their grid without requesting foreground row rasterization.

`just toolkit-fonts-test` checks shared/platform precedence, symlink preservation,
transactional fallback and window preview/apply/cancel/reset without a display.
`just toolkit-ui-test` exercises real six-pane font changes, matching PTY geometry,
unchanged items/PIDs/input, dotfile preservation, modal scope, menu/Commands access,
and minimum-width layout on X11 and private Wayland. Captures are `font-preview.png`
and `font-narrow.png` in `.local/artifacts/qt-ui/{xcb,wayland}/`. Native Mac review,
external-display sharpness and saved-window restoration remain open. The daily
viewer is still `just spike`.
