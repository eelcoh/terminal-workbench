# Appearance spike

The workspace client now has shared surface, text, focus and status colors, rounded
controls, and a live appearance picker. These are the first runnable UX slice;
owner review and native platform validation are still required.

Open **Appearance** at the bottom of the workspace sidebar, or **Theme** in the
overview. Browse terminal sample cards, type to search names, and choose **All**, **Dark**
or **Light**. Click a card or use the arrow keys to preview it. Scroll the grid
in smaller windows. Backspace edits the search; Ctrl+A (Command+A on Mac) or the
search clear button clears it. A search with no matches keeps the picker open
and disables Apply. Your running terminals, previews and chrome change together. **Apply** / Enter
saves the colors; **Cancel** / Escape restores the exact colors from before opening.
Theme changes do not restart shells or change PTY geometry. Output continues while
the picker owns input. Save failures keep the picker open so you can retry or cancel.

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

An imported palette appears as **Custom colors**, unless its supported colors
exactly match a bundled palette. Saved colors are matched to the gallery when
reopening. This environment variable takes
precedence over saved appearance on every launch. Unset it to use the saved colors.
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

Apply writes `appearance.conf` in the launcher state directory (currently
`.local/sessions`) using a temporary file and atomic replacement. Workspace/session
organization remains separate. The saved file contains resolved colors, including
all 256 palette entries; an import remains available in the current picker until
restart. The bundled palettes are embedded at build time; only an explicitly imported
file or saved preferences require appearance file reads at startup. The
single-terminal renderer diagnostic has no appearance picker.

## Evidence and remaining work

`just theme-gui-test` exercises one and fifteen real terminals under Xvfb: live
light/dark preview, cancellation, persistence/reopen, failed saves, imported fields,
malformed-import isolation, unchanged process/socket identities and PTY geometry,
and prevention of modal text reaching a shell. It also checks gallery search,
dark/light filters, no-match behavior, Dracula/Latte apply/reopen, narrow-window
navigation, and the same PTY geometry while the picker window is resized. It saves screenshots under
`.local/test-results/ux-*` and samples idle overview CPU.

A larger theme catalog, font/typography controls, application color overrides,
system appearance and accessibility semantics remain open. The loader has synthetic
format/error checks and parses all five bundled official Ghostty files;
compatibility beyond the documented color subset is not claimed. Native macOS and physical Linux desktop review must establish
visual quality and interaction feel before the UX milestone can be called complete.
