# Drop-in Ghostty themes

In `just toolkit-ui`, put Ghostty theme files in your Workbench configuration
folder’s `themes/` directory. Use **Appearance → ? → Open themes folder** to open the
actual location in your file manager. The help icon keeps the folder instructions
out of the theme list. Larger samples show each theme’s actual background,
foreground and palette; the live workspace preview is not dimmed. The directory is created when needed.

| Platform | Default folder |
| --- | --- |
| Linux | `~/.config/terminal-workbench/themes/` |
| macOS | `~/.config/terminal-workbench/themes/` |

Both platforms honor `XDG_CONFIG_HOME`. On either platform, `WORKBENCH_CONFIG_DIR` selects
a different configuration root; themes then live in its `themes/` directory.
This can point to a shared dotfiles checkout. File and directory symlinks work.

1. Download the **Ghostty** format from terminalcolors.com.
2. Drop the downloaded file into that directory. Keep its `.default` suffix.
3. Reopen Appearance, search for its name, preview it and choose **Apply**.

`.default`, `.theme`, `.conf`, `.ghostty` and extensionless files all work: contents
determine whether a file is a supported Ghostty palette. No renaming, import form,
URL, metadata entry or network access is required. **Cancel** restores the previous
workspace theme. Interface colors and Overview previews follow the workspace.

Appearance rescans when opened, so adding or editing files does not require an
app restart. Source files stay untouched; discovery does not create copies or an
import manifest. Previously imported themes remain in the list, but the import form has been removed. Removing
or editing a file does not recolor an already-themed terminal: existing colors
remain in the window until you choose a replacement. New selections use the
updated file. Choices remain temporary in `just toolkit-ui`. Saved `just toolkit-workbench`
windows retain committed colors in their separate view profiles, even when
the original file is removed; reselect a changed file to adopt its new colors.

Only supported color fields are applied. Shell commands and other non-color
settings are ignored. Invalid or unreadable files are skipped with a status in
Appearance; other themes remain usable. Each file is limited to 64 KiB, with up
to 256 directory entries scanned per opening. Identical file contents are shown
once. The existing offline presets remain available.

`just toolkit-theme-import-test` covers discovery, suffixes, refresh, source-file
preservation, edits/removal, symlinks, invalid files and the optional import store.
`just toolkit-ui-test` covers real `.default` preview/Apply/Cancel and removal,
unchanged terminal processes/geometry and the existing UI on X11 and Wayland.
Native Mac folder opening and discovery still need owner review. `just spike`
remains the daily-use frontend; this directory workflow belongs to the Qt candidate.

On macOS, startup copies existing preferences from the previous
`~/Library/Application Support/Terminal Workbench` location once. Managed config,
themes and Mac UI settings are copied only where a destination does not already
exist; existing destination symlinks are left alone. Original files stay intact,
and session/runtime data is never copied. Explicit `WORKBENCH_CONFIG_DIR` overrides
skip this migration. A marker prevents removed settings/themes from reappearing.
