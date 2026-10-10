# Bundled palette sources

The Ghostty files are copied unchanged from their official repositories. All
fetches used SSH. Their MIT licenses are preserved alongside the themes.

| Collection | Source revision | Files |
| --- | --- | --- |
| [Catppuccin Ghostty](https://github.com/catppuccin/ghostty) | `b0b03ccee7ae8f16b13bd4fdfe267616defdb2b7` | Mocha, Macchiato, Frappé, Latte (`themes/*.conf` upstream) |
| [Dracula Ghostty](https://github.com/dracula/ghostty) | `b0e64590232331d9837c82cc1c5138e11ea39f76` | `dracula` |

Workbench Night/Dawn remain original project palettes. TerminalColors is a
browsing reference; no website imagery or theme downloads were copied from it.
The browser and swatches render locally and require no network at launch.

`scripts/prepare-theme-catalog.py` embeds these exact source files into a C header;
the normal Ghostty color reader parses them once at client initialization. Only
supported color fields apply. Cursor text/split divider options stay unsupported.
When updating this collection, pin source revisions, keep notices, regenerate the
header and run `just check` and `just theme-gui-test`. There is no automatic update.
