# Appearance spike

The workspace client now has shared surface, text, focus and status colors, rounded
controls, and a live appearance picker. These are the first runnable UX slice;
owner review and native platform validation are still required.

Choose **Change appearance** in the hamburger menu or **Theme** in Overview.
The picker now chooses a theme for the active workspace. Its title names that
workspace, and **Apply to workspace** makes the destination explicit. There are
no Application/Workspace/Terminal/Defaults tabs or follow checkbox.

Browse sample cards, type to search, and filter **All**, **Dark** or **Light**.
Click a card or use arrows to preview. The workspace's inheriting terminals and
chrome preview together; the picker keeps the surface colors captured at opening
so its controls stay readable. **Cancel**, Escape or the close icon restores the
entire previous appearance. Save failures keep the preview open for retry/cancel.
Tab traverses search, gallery, close, filters, clear, reset and Cancel/Apply.
Backspace edits search; Ctrl+A (Command+A on Mac) and the clear button clear it.
Apply pins the selected palette even when it currently matches the default.
Apply is disabled when no theme matches. **Use default theme** previews removing
the workspace override; Apply saves the reset.

Opening the workspace picker previews matching chrome by default; Apply saves
that follow preference, and Cancel restores its previous value. An explicit
dotfile `follow-workspace = false` requests fixed chrome and is respected by the
picker. Application colors and default terminal colors remain advanced dotfile
settings. Existing colors/follow preferences are loaded without being flattened
or rewritten merely by upgrading or opening a window.

### Terminal exceptions

A pane's three-dot menu opens **Terminal theme…**, a separate picker for that
terminal's custom colors. **Use workspace** previews clearing the exception.
**Apply to terminal** saves it; Cancel restores it. A terminal exception never
changes chrome. The main workspace picker quietly reports how many terminals
keep custom colors, rather than presenting another scope to choose.

Terminal colors still resolve as terminal exception → workspace → default.
New terminals inherit the workspace, and empty workspaces can be themed before
their first terminal exists. Themes follow stable IDs through renaming/reordering;
moving an exception keeps its colors and moving an inheriting terminal adopts
its destination workspace palette. Closed entities' preferences are pruned when
organization is reconciled. Catalog refresh remains deferred during a dialog,
and edit targets are captured by ID, so a pending update cannot redirect the edit.

Palette changes never restart shells, resize PTYs, clear selection or send dialog
input to terminals. Every Overview thumbnail retains its terminal's colors.
Global/scoped persistence formats and theme-file compatibility remain unchanged.

The offline gallery includes:

- Workbench Night and Dawn (original project palettes).
- Dracula.
- Catppuccin Mocha, Macchiato, Frappe and Latte.

Each card shows its own terminal colors and 16 ANSI swatches, even while another
palette is previewed. The five third-party palettes are unchanged official
Ghostty files, with pinned source revisions and MIT notices preserved in
[the bundled sources](../themes/README.md). The same loader parses bundled and
imported files. The app needs no website, theme downloads or Ghostty installation
at launch. TerminalColors inspired the browsing flow; its images were not copied.

## Ghostty color files

Import a local theme when launching:

```sh
WORKBENCH_THEME=/absolute/path/to/theme just spike
```

`WORKBENCH_THEME` overrides default terminal colors on each launch; explicit
workspace/terminal palettes retain priority, and application appearance stays
independent. A non-bundled imported palette is available as **Imported colors**
in the workspace or terminal picker. A target's existing custom palette appears as **Custom
colors**. Unset the environment variable to use saved defaults. Saved colors are
matched to bundled palettes when reopening.
The loader accepts this subset of Ghostty configuration syntax:

| Field | Supported value |
| --- | --- |
| `background`, `foreground` | Six hexadecimal digits, with optional `#` |
| `cursor-color` | Same RGB syntax |
| `selection-background`, `selection-foreground` | Same RGB syntax |
| `palette` | Index 0–255 followed by `=` and an RGB value; repeat for each entry |

Whitespace, whole-line `#` comments and double-quoted RGB values are accepted.
Missing fields retain Workbench Night defaults. Unsupported options are counted and
ignored; they cannot run commands or load other configuration files. Malformed
supported values, oversized lines, or files containing no supported colors reject
the import before attaching to sessions. Existing shells and saved appearance stay
intact. This is color-field compatibility, not a complete Ghostty configuration loader.
Named colors, inline comments, includes, cursor text color, fonts, opacity and system
light/dark following are outside this first slice.

Normal launches now load portable shared/platform global colors followed by
separate UI overrides. Automatic chrome-follow changes save only the follow field outside the
checkout;
workspace/terminal bindings remain local to the session directory. See
[dotfiles](dotfiles.md) for configuration, migration and precedence. The following
legacy state format still holds scoped palettes and supports standalone test clients.

Apply atomically replaces `appearance.state` in the launcher state directory
(currently `.local/sessions`). It has a versioned client preference format and
stores resolved RGB values for application/default/workspace/terminal palettes,
including all 256 palette entries, plus follow mode. A malformed file is rejected
before attaching; it is not overwritten. This is separate from `workspaces.json`.

When `appearance.state` does not exist, an older `appearance.conf` is loaded as
both application and default terminal colors, preserving the previous look.
The next Apply writes the new format and leaves the old file intact. Once the new
file exists it takes priority. The imported Ghostty format itself stays unchanged.

Bundled palettes are embedded at build time. Only saved preferences and an explicit
import require appearance reads at startup. Colors apply when appearance or
organization changes; switching a workspace in follow mode updates chrome without
reapplying every terminal palette. The single-terminal diagnostic has no picker.

## Evidence and remaining work

`just theme-gui-test` exercises one and fifteen real terminals under Xvfb: live
light/dark preview, cancellation, persistence/reopen, failed saves, imported fields,
malformed-import and malformed-state isolation, legacy preference migration,
real per-pane light/dark color-scheme replies, unchanged process/socket identities
and PTY geometry,
and prevention of modal text reaching a shell. It also checks gallery search,
dark/light filters, no-match behavior, Dracula/Latte apply/reopen, narrow-window
navigation, mixed-color overview previews, stable/follow chrome, scoped
Apply/Cancel, inheritance reset, moved override/inherited terminals, and the same
PTY geometry while the picker window is resized. It saves screenshots under
`.local/test-results/ux-*` and samples idle overview CPU.

A larger theme catalog, font/typography controls, application color overrides,
system appearance and accessibility semantics remain open. The loader has synthetic
format/error checks, scoped roundtrip/corruption/limit tests, and parses all five
bundled official Ghostty files;
compatibility beyond the documented color subset is not claimed. Native macOS and physical Linux desktop review must establish
visual quality and interaction feel before the UX milestone can be called complete.


## Qt candidate: drop-in themes

For `just toolkit-ui`, the primary flow is now **Appearance → ? → Open themes folder**.
Drop Ghostty files into the configuration `themes/` directory, retaining
TerminalColors’ `.default` suffix, and reopen Appearance. Preview and Apply use
the existing workspace flow; Cancel restores the old colors. The link opens a
file manager, not a selector; files in the folder appear automatically when
Appearance reopens. The import form has been removed. See [theme directory](theme-directory.md) for Mac/Linux paths.
