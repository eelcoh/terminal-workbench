# SSH hosts (v0.3 design note, 2026-11-07)

Status (2026-11-07): slices 1–3 built, plus Band's own host list.
- `[host.ID]` in config.conf, and Connect to… in Commands.
- Host panes with their own identity, outside the Apps list.
- The hosts filter in the sidebar (since 2026-11-09; before, a Hosts panel):
  host panes in their workspaces, then the saved hosts. Before that, the Hosts
  panel (sidebar shape A): saved hosts with their open panes in any
  workspace, Connect, and the footer filter.
- New host…, which writes `ui/hosts.json` and suggests `~/.ssh/config`
  aliases. Hosts Band saved can be removed from the panel.

- Reachability in the Hosts panel: `ssh -G`, then a two-second TCP
  connection, on opening and every minute while the panel is open. A ready
  dot means reachable, a waiting dot means not reachable, and no dot means
  unknown (proxied, or the lookup failed).

- A theme per host: a host pane's terminal takes the host's theme, by name
  from the theme list, unless it has its own colors. New host… offers
  Workspace colors or a theme.

All five slices are built. A remote runtime, and with it the host level in
the sidebar, comes later ([evaluation note](remote-runtime.md)). The owner's decisions are at the
end.

Rex treats hosts as the top level of its sidebar (This Mac, then remote hosts
with a reachability mark). Band's v0.3 goal is the same everyday use, plain SSH
panes in the strip, without a daemon on the remote machine. A remote runtime
(the session service on the far side, the client local) stays a later
evaluation, as the roadmap says.

## What exists today

- A pane whose foreground process is `ssh`, `mosh-client`, `telnet` or
  `rlogin` is marked remote. Its context is `remote-unavailable`: the local
  directory is never shown as the remote folder, and the caption reads
  "Remote session". The program name (`ssh`) shows in the header since WAT4.
- Command terminals exist for apps: a pane started with a command, its own
  name, kept with its last lines when the command fails, and Start again.
- Screen output cannot change trusted context: an OSC 7 directory report from
  the shell is ignored, by design and by test (`tests/context_smoke.py`).
- Per-terminal colors exist (Terminal colors…), saved per terminal ID.

## Proposal

**A host** is a saved SSH destination. Each host has:
- an ID and a display name;
- a destination, which may be an `~/.ssh/config` alias;
- optionally a remote folder to start in;
- optionally a terminal theme.

All of the SSH options themselves stay in `~/.ssh/config`, which Band does not
write.

```text
[host.build]
name = Build box
destination = build          # an ssh alias, or user@host
directory = ~/src/band       # optional: cd there, then a login shell
theme = Dracula              # optional: colors for every pane on this host
```

**A host pane** is a command terminal running
`ssh -t -- <destination>`, or with a directory
`ssh -t -- <destination> 'cd <dir> && exec "$SHELL" -l'`. It reuses the app
machinery:
- when ssh exits, the pane keeps its last lines and offers Start again;
- after a restart it is a stopped terminal like any other, started again
  explicitly.

Band never stores passwords or keys: ssh prompts in the pane, and agents and
keychains work as they do in any terminal. Destinations are validated, with no
leading `-` and no whitespace or control characters, and passed after `--`.

**Identity and context.** A host pane's identity is the host name: a host
icon, the name, then the place.
- The place is the trusted local fact "remote" plus, when the remote shell
  sends one, an OSC 7 folder.
- That folder counts only when its hostname matches the host. It is shown as
  reported by the remote shell, in muted text and kept separate from trusted
  context.
- It never feeds search ranking, workspace folders or Git marks.
- Without OSC 7, the place is the terminal title the remote shell sets, or
  nothing.

**Reachability.** The window asks `ssh -G <destination>` for the resolved
hostname and port, then tries a TCP connection with a two-second limit:
- this runs once a minute while the Hosts list is visible, and on demand;
- the result is reachable, unreachable (shown as a warning mark) or unknown;
- `ProxyJump` or `ProxyCommand` hosts are always unknown, since Band does not
  run proxies to check them.

Nothing logs in, and no remote command runs for this check.

**Colors per host.** A host pane takes its host's theme as its terminal
colors unless that terminal has explicit colors of its own. This uses the
existing per-terminal appearance; it is not a new color layer.

**Sidebar.** Two shapes fit the owner's navigation model (typed views beside
the grouped Workspaces view):

- **A. Hosts as a typed view.** A rail icon and panel like Apps:
  - saved hosts with reachability, each with its open panes;
  - Connect opens a host pane in the current workspace;
  - New host… adds a host.

  The Workspaces panel marks host panes with the host icon and name. The
  footer filter matches host names.
- **B. Hosts as a level in Workspaces**, as in Rex: This computer, then each
  host, then its workspaces, then panes. This makes a host a container of
  workspaces, which Band's model does not have: a workspace is local and can
  mix panes from several hosts.

Recommendation: A. It keeps workspaces as they are (a strip that can hold a
local shell, an app and two SSH panes side by side), and it matches how Apps
and Agents already work.

## Slices

1. `[host.ID]` in config.conf (parser, platform overrides, validation) and
   host panes started from Commands ("Connect to Build box").
2. Host identity in headers and sidebar rows (icon, name, reported folder
   kept apart), Apps excluding host panes.
3. The Hosts panel: saved hosts, open panes, Connect, New host… writing
   nothing but Band's own host list (see question 1).
4. Reachability with `ssh -G` and a TCP check; the filter matches hosts.
5. Theme per host.

Each slice ships with tests: parser and validation in C, the reachability
check against a local listener, the UI test with a fake `ssh` on PATH, and
by-hand runs against a real host on both platforms.

## Decisions (owner, 2026-11-07)

1. **Where hosts live:** both. `[host.ID]` in config.conf, which Band reads
   and never writes, and a host list Band saves itself (beside the font,
   shortcut and theme files) that New host… writes. Band's list wins for the
   same ID, as with the default theme.
2. **`~/.ssh/config`:** New host… suggests its `Host` aliases (no wildcards),
   read only.
3. **Sidebar shape:** it depends on what Band restores.
   - **Plain ssh panes (v0.3):** a host pane belongs to its workspace, like
     any terminal, so hosts are a typed view (shape A, built). Workspaces stay
     local and may mix panes from several hosts.
   - **A Band daemon on the remote host (later):** Band restores the
     connection to that runtime rather than an ssh process. Then a host holds
     its own workspaces, and the host level in the sidebar (shape B) should be
     reconsidered together with the remote runtime.
4. **After a restart:** host panes come back stopped and connect on Connect
   again, like shells; no automatic reconnect.
