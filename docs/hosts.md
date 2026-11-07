# SSH hosts (v0.3 design note, 2026-11-07)

Status: proposal. Nothing here is built yet; the open questions at the end need
the owner's answer before the first slice.

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

## Open questions for the owner

1. **Where hosts live.** Options:
   - only `[host.ID]` in config.conf, which Band reads but never writes;
   - a host list Band saves itself, beside the font, shortcut and theme
     files, which New host… can then write;
   - both, as with the default theme, where Band's file takes precedence.

   Recommendation: both.
2. **Suggest from `~/.ssh/config`.** Offer its `Host` aliases (no wildcards)
   in New host…, read only? Recommendation: yes.
3. **Sidebar shape.** A (typed Hosts view) or B (host level in Workspaces)?
   Recommendation: A.
4. **After a restart.** Host panes come back stopped and connect on Start
   again, like shells (recommended), or reconnect automatically?
