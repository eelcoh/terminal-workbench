# Appearance spike

The workspace client now has shared surface, text, focus and status colors, rounded
controls, and a live appearance picker. These are the first runnable UX slice;
owner review and native platform validation are still required.

Choose **Change appearance** in the hamburger menu, or **Theme** in the overview. A pane’s three-dot menu opens **Terminal theme**
with that terminal’s scope selected.
The picker has four targets:

| Target | What changes |
| --- | --- |
| Application | Sidebar, dialogs, overview surfaces and other chrome |
| Workspace | Terminal colors for the active workspace |
| Terminal | An override for the focused terminal, such as a remote shell |
| Defaults | Fallback terminal colors for workspaces without a theme |

Terminal colors resolve in this order: terminal override → workspace palette →
default terminal palette. The application palette is separate. Stable chrome
is the default: **Follow active workspace** starts unchecked. Checking it makes
application colors follow the
workspace palette, including its inherited default. A terminal override never
changes chrome in this mode. Choosing an Application palette returns to stable
chrome. This gives you project/machine colors while keeping the overview coherent.

Browse sample cards, type to search names, and filter **All**, **Dark** or **Light**.
Click a card or use arrows to preview. Scroll smaller grids. Backspace edits the
search; Ctrl+A (Command+A on Mac) or the clear button clears it. With no matches,
Apply is disabled. **Use defaults** removes a workspace override;
**Use workspace** removes a terminal override.

Switching targets keeps your pending changes. **Apply** / Enter saves the entire
appearance dialog; **Cancel** / Escape restores all palettes, inheritance and
follow mode from before opening. Save failures keep the preview open for retry or
cancel. Output continues while the picker owns input. Palette changes do not
restart shells, change PTY sizes, clear selection or send typed input to terminals.
Every overview thumbnail retains that terminal's own colors.

Themes follow stable IDs through renaming and reordering. Moving a terminal with
an override keeps its colors; moving an inheriting terminal adopts the destination
workspace palette. Newly created terminals inherit their workspace. Closed entities'
preferences are pruned when organization is reconciled. Themes do not change shared
runtime/session metadata or detect remote hosts automatically.

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
for any picker target. A target's existing custom palette appears as **Custom
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
separate UI overrides. Global Apply saves only edited fields outside the checkout;
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
