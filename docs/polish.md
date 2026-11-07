# Polish: notes from Rex (2026-11-07)

Rex (Mitchell Hashimoto) launched as a similar app: a sidebar of hosts,
workspaces and panes, agents, remote hosts. The owner shared nine screenshots
(first run and daily use, macOS). Its polish is ahead of Band's; its feature
set is not: it has no apps catalog, and the screenshots show tiled splits in a
session, not a scrolling strip, and macOS only. These notes are inspiration,
read from screenshots only; nothing here is copied, and Band's shortcuts stay
its own.

## What the screenshots show

- **First run in the main area, not a dialog.** Five steps with Back and
  Continue (Continue also on Space, shown as a key chip in its tooltip):
  look and feel (color scheme Auto/Light/Dark as small window previews,
  interface style, sidebar position, theme cards showing each palette);
  integrations and permissions (install its agent skill, grant system
  permissions); migrate settings by copying a prompt for your coding agent
  (keybindings from other terminals and tmux); and "You're all set" with three
  shortcuts read from your keymap.
- **One sidebar tree.** Host ("This Mac") → workspace label (small, muted) →
  panes as rows with a process icon, the process and the folder (`zsh ~`).
  Remote hosts sit below with a status mark (a warning when unreachable). The
  selected row is a filled pill with a close button; hovering a row shows a
  popover listing that session's panes.
- **Bottom of the sidebar:** a filter field and a host menu (All hosts, This
  Mac, each host with an icon).
- **Title bar folded into the window.** Traffic lights, sidebar toggle and a
  few icons (palette, new) sit in the sidebar's top; there is no separate
  toolbar.
- **Pane chrome.** A header with icon, process and folder; on the focused pane
  a row of small actions (split right, split down, zoom, close) at the right.
  Unfocused panes are dimmed slightly. Thin gaps, rounded corners.
- **One visual system.** A warm dark theme with a single accent (amber) for
  primary buttons, selection outlines and badges; pill buttons; key chips;
  line icons in one weight; a clear type scale (bold heading, muted subtitle).

## Where Band already differs

- Apps are first-class: a catalog of TUIs, installed checks, installs through
  Homebrew, the system package manager or COPR, a Sources view
  ([apps](apps.md)).
- The niri-style scrolling strip with Overview, rather than splits.
- Linux and macOS both, from one client.
- Typed lists (Agents, Apps) beside the grouped Workspaces panel.

## Proposed polish, in order

1. **Pane identity.** A kind icon (shell, agent, app) and "process · folder"
   in pane headers and sidebar rows, one component for both. *Done:* the
   runtime's WAT4 watch adds the foreground executable's file name; headers
   and Workspaces rows show icon, program and folder.
2. **Key chips everywhere.** Shortcuts as chips in the command palette, menus,
   tooltips and the shortcut help, from the live keymap. *Done:* one KeyChips
   component (a chip per key, ⌘⇧ symbols split on macOS) in the palette and its
   footer, menus, the shortcuts help and button tooltips.
3. **One accent and selection style.** A single accent per theme for primary
   actions, selection and focus; pill rows in the sidebar; line icons extended
   to the actions we show (split/width, zoom, filter, host).
   *Done (first pass):* a primary button style filled with the theme's accent
   (text in whichever theme color contrasts more) for the one main action of a
   view (catalog Open/Install, Install Homebrew, Apply, Create/Save); the current
   row in lists uses the accent-tinted selection the sidebar already used, hover
   stays neutral, keyboard focus is an accent ring; the catalog footer uses key
   chips. Confirmations of destructive actions keep a quiet confirm button.
   Still open: icons for pane actions (with item 4), and the Appearance and
   theme-import dialogs, which still use fixed colors.
4. **Pane actions on the focused pane.** Width, zoom (full width) and close as
   small header buttons on the focused pane, beside the existing menu; dim
   unfocused panes a little (respecting reduced motion and contrast).
5. **First run in a pane.** Appearance (Auto/Light/Dark, theme cards with live
   previews, sidebar pinned or not), agent launchers, *pick your apps* from the
   catalog (Band's own step), and the keys worth knowing from the keymap.
   Settings import stays optional; Ghostty theme import exists already.
6. **Sidebar filter.** One filter field at the bottom of the sidebar across
   workspaces, panes, agents and apps; hover previews of a closed workspace's
   panes.
7. **Window chrome.** Fold the top bar into the title bar (macOS traffic
   lights inline; client-side decorations on Linux). Platform work with risk;
   last.
8. **Hosts (v0.3).** When SSH hosts arrive: a host level in the Workspaces
   panel with reachability, and a host filter.

Each item ships on its own with screenshots on both platforms, like the apps
slices.
