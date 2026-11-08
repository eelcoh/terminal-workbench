# Remote runtime (evaluation note, 2026-11-08)

Status: an evaluation, nothing built. It follows the plain SSH panes of v0.3
([SSH hosts](hosts.md)), as the roadmap says. The owner's answers are at the
end (2026-11-08); whether to build it is still open.

## What it would add

A plain host pane is an `ssh` process in a local runtime. When the window,
the network or the laptop goes away, the remote shell goes with it, and Band
knows nothing about what runs on the far side beyond "Remote session".

With a remote runtime, the session service and the terminal runtimes run on
the remote host, and the client attaches to them over SSH:
- Shells survive a closed lid, a dropped network or a client restart, as local
  terminals survive a closed window today. Reconnecting attaches; it never
  starts a process.
- The facts Band already computes come from the far side: the agent and its
  status (WAT4), the program name, the trusted folder and Git branch and dirty
  mark (CTX2), the stopped-shell reasons, scrollback search.
- A host holds its own workspaces: the remote catalog. This is the case in
  which the owner asked to reconsider the host level in the sidebar.

Plain host panes stay as they are. They remain the right thing for a quick
login, a machine without Band, or a hop through a jump host where nothing may
be installed.

## How Band talks to its runtime today

Every connection is a Unix socket in one private state folder:

| Endpoint | Used for |
| --- | --- |
| `service.rpc` | Catalog changes, health, status, recovery: one JSON line per connection |
| `<terminal>.ctl` | Attachment (ATS1/ATM1, then a long-lived stream), WAT4 and CTX2 watches, CAPS, STOP, TEXT/SNAP |
| `<terminal>.sock` | Legacy replay attachment |
| `workspaces.json` | Read by the client after a file notification (the catalog feed) |

One open terminal holds about three long-lived connections (the stream and two
watches) and opens short ones for status and actions. A window with 20 remote
terminals would hold about 60 connections to the host.

The client finds all of these from a single `state` path in its
configuration. `UiSession`, `UiCatalogFeed` and `RuntimeFacts` each join that
path with a fixed name. A second state folder per host is therefore the
smallest change on the client side, provided something local answers on it.

## Transport options

1. **A TCP or TLS listener in the remote runtime.** Rejected. It adds a network
   service with its own authentication, keys and exposure, and the runtime's
   methods are same-user hints, not an authentication boundary (see
   [spike protocol](spike-protocol.md)).
2. **OpenSSH Unix-socket forwarding** (`ssh -L local.sock:remote.sock`, added
   per socket through a ControlMaster with `ssh -O forward`). Nothing to
   install on the local side, but:
   - one forward per endpoint, added and removed as terminals come and go;
   - the server can disable it (`AllowStreamLocalForwarding`);
   - stale local socket files need `StreamLocalBindUnlink`;
   - the client cannot tell a dropped forward from a stopped terminal.
3. **A remote command per connection** (`ssh host band-session connect
   NAME`, which pipes stdio to one socket, through a ControlMaster). Simple,
   but each connection is an SSH session, and OpenSSH allows 10 sessions per
   connection by default (`MaxSessions`), well under the 60 above.
4. **One bridge per host** (recommended). The client side runs
   `ssh -T -- <destination> band-session bridge`, once per host. Over that
   single SSH session, a small multiplexing protocol opens a channel per
   connection to a named remote endpoint. A local half of the same program
   listens on Unix sockets in a private per-host state folder
   (`<state>/hosts/<id>/`) and maps each accepted connection to a channel.

Why the bridge:
- the client code stays nearly as it is: it is pointed at another state folder;
- one SSH login per host, with the same agent, keys, `ProxyJump` and prompts as
  a host pane;
- no listening port on either machine, and no OpenSSH server settings beyond
  running a command;
- one process whose exit means "host disconnected", so the client can tell
  that apart from a stopped terminal.

Its cost is the multiplexing protocol, the main new piece. It needs:
- per-channel flow control, so a terminal flooding output cannot stall another
  terminal's input or a status reply;
- bounded queues as elsewhere, with a slow channel losing only itself;
- named endpoints only: `service.rpc`, `<terminal-id>.ctl` and
  `<terminal-id>.sock` in the remote state folder, validated as terminal IDs
  are today. The bridge refuses paths, so it never becomes a general socket
  forwarder.

Both halves would be Rust in `crates/runtime`, next to the service, and
testable on one machine with `ssh` replaced by a local spawn.

### Tailscale

Tailscale was raised as an alternative (owner, 2026-11-08). It can play two
parts:

- **The network under SSH.** The bridge runs `ssh` to a tailnet name, with
  OpenSSH or Tailscale SSH on the host. Nothing in Band changes. The host is
  reachable from anywhere without port forwarding. Its tailnet address stays
  the same when the laptop changes networks, so an idle SSH session can survive
  a Wi-Fi change that would otherwise drop it.
- **The transport itself.** The remote runtime would listen on its tailnet
  address, and each client connection would be its own TCP connection, so no
  multiplexing protocol is needed. The runtime would have to check every
  peer's identity with the local Tailscale daemon and accept only the same
  user. That makes Band depend on Tailscale on both machines. It also trusts
  the tailnet's access rules, and on a shared tailnet those are not the user's
  alone. Every user without Tailscale would still need the SSH path.

Verdict: Tailscale is better as the network under SSH than as a replacement for
it. The SSH bridge works over Tailscale unchanged and gets its reachability and
stable addresses for free. A direct tailnet transport removes the multiplexing
protocol but adds a second way in, with identity checks Band would own. If the
bridge proves slow or fragile, the bridge's named endpoints leave room to add
it later. None of this is tested: there is no tailnet here yet.

### The catalog file

The client watches `workspaces.json` on disk, which does not exist locally for
a remote host. The service already answers `catalog.read`. Remote hosts need a
change notification instead of the file watch, either:
- a `catalog.watch` method on the service, a subscription like WAT4 that sends
  a revision on each change and stays open; or
- the local bridge half watching nothing and polling `catalog.read`.

A `catalog.watch` is the cleaner one and also serves local windows later.

## Installing and versions

The remote host needs Band's runtime binary, and it has to match the client:
- **Snapshot attachment** is negotiated by the exact Ghostty VT revision.
  With a different revision the client falls back to legacy replay, which is
  limited to 16 MiB and loses state after that.
- **Watch and context protocols** fall back gracefully (WAT4 to WAT2, CTX2 to
  CTX1), so small version skew there is safe.

Two ways to get it there:
- **The user installs Band on the host** (a package, or the runtime alone).
  The client checks `band-session --version` and the VT revision on connect
  and explains a mismatch. This fits how Band ships today.
- **The client copies its runtime over**, into a versioned folder on the host
  (`~/.local/share/band/<version>/`), as editors with remote servers do. This
  needs Linux builds per architecture (x86_64, aarch64) and macOS builds of the
  runtime alone, signed and checked on arrival. It is more convenient and much
  more to maintain.

Decided (owner, 2026-11-08): install Band on the host. The runtime comes with
it, and the version check on connect explains a mismatch. A full install pulls
in Qt on a headless server; a runtime-only package can come later if that gets
in the way.

### Staying alive on the host

Band's runtimes already start in their own session (`setsid`), so they outlive
the SSH session that started them. Two host settings can still end them:
- **Linux:** systemd-logind with `KillUserProcesses=yes` stops everything in a
  user's session when they log out, whatever the process group. With it set,
  `loginctl enable-linger`, or a systemd user service for `band-session
  service`, keeps them. The bridge should detect a killed service and say
  which, not just "disconnected".
- **macOS:** processes started over SSH live outside the logged-in GUI session.
  Shells work, but the clipboard and `open`, and some keychain access, behave
  differently than in Terminal on that Mac. A Mac that sleeps also drops every
  connection until it wakes.

## Security

- **Authentication** is SSH alone. Band stores no credentials, as with host
  panes.
- **Exposure:** the remote sockets stay mode 0600 in a user-owned state folder,
  as today. The local per-host folder gets the same checks as the local state
  folder: user-owned and not writable by others.
- **What a remote host can claim.** Facts from a remote runtime are as
  trustworthy as the remote account, and no more. A compromised host can send
  any frame its protocol allows. The client parsers are already bounded, and
  remote facts must also stay remote:
  - a remote workspace folder is never used as a local working directory, file
    dialog start, or worktree root;
  - remote agent and context facts are labelled with the host, and are never
    merged into local search ranking or local Git marks;
  - local actions on a remote terminal (Open folder, worktrees, Apps launches)
    are hidden or run on the host, never run locally with a remote path;
  - theme and app names from a remote catalog are looked up in the client's
    own lists only.
- **Agent hooks** report to their own runtime on the host (`STATE` on the
  remote `.ctl`), so they need no change and cross no machine boundary.

## What changes in the client

- **A session per host.** `UiSession` and the catalog feed run once for the
  local state and once for each connected host. Terminal identities are
  qualified by host in the client, since two machines can both have
  `main-1`.
- **Workspaces.** A workspace belongs to one machine: the remote catalog's
  workspaces are the host's workspaces. A local workspace can still hold plain
  host panes, as in v0.3.
- **The sidebar.** This is where the host level (shape B in
  [SSH hosts](hosts.md)) returns: This computer, then each connected host, each
  with its workspaces and panes, a reachability mark and a connection state.
  The Hosts panel stays the place to add hosts and open plain host panes.
- **Restore.** After a client restart, Band remembers which hosts were
  connected and which remote workspaces were open, and reconnects to them
  automatically with backoff (decided). Reconnecting only attaches; unlike
  plain host panes, nothing starts. An SSH prompt (a passphrase, a new host
  key) pauses the retries until the user answers it in a visible place.
- **Disconnects.** When the bridge exits, every terminal on that host shows
  "Disconnected from Build box", with Reconnect. This is separate from a
  stopped terminal, which still offers Start new shell only after the remote
  service confirms it.
- **Things that stay local:** fonts, themes, shortcuts, window layout, focus
  and selection, as today; they never travel to the host.

## Latency and size

- Typing goes through one SSH session, like a host pane; the bridge adds a
  frame header per write and no round trips.
- Attaching sends a complete snapshot, which can reach tens of MiB with long
  scrollback and is slow on a poor link. SSH compression helps. Incremental
  history (already future work for local attachment) helps more, and becomes
  more important here.
- The context worker runs `git status` on the host, so a slow remote disk
  costs the host, not the client.

## Alternatives that need no Band work

- A plain host pane running tmux or zellij on the host keeps shells across
  disconnects today, but Band sees one terminal and none of its facts.
- mosh helps with roaming and latency for a host pane, and does not carry
  sockets.

These cover persistence for people who need it now; the remote runtime is
about Band's own model reaching the host.

## Slices, if the owner goes ahead

1. **Bridge:** both halves, the multiplexing protocol with flow control and
   named endpoints, tests on one machine with a fake `ssh`.
2. **`catalog.watch`** on the service; the local feed can use it too.
3. **Read-only host session:** connect a host, list its workspaces and
   terminals in the sidebar, attach and type. The version and VT revision
   check, with a clear message on mismatch.
4. **Remote changes:** create, rename, close and recover terminals and
   workspaces on the host, through the same service methods.
5. **Sidebar host level and restore:** connected hosts after a restart,
   reconnect, disconnect states, remote facts kept apart.
6. **By hand** against a real Linux host from both Linux and macOS clients,
   then a macOS host (decision 5).

A rough size: slice 1 is the largest and riskiest (about the size of the
session service port); slices 3 and 5 touch most of the client.

## Decisions (owner, 2026-11-08)

1. **Go ahead:** left for later (owner, 2026-11-08). Tailscale was raised as an alternative and is
   weighed above: better as the network under the SSH bridge than as a
   replacement. It is untested until there is a tailnet to try it on.
2. **Installation:** install Band on the host; the runtime comes with it.
3. **Reconnect:** automatic, with backoff.
4. **Workspaces:** a workspace belongs to one machine.
5. **Which hosts:** follows from 2. Installing Band works on both platforms, so
   Linux and macOS hosts are both in scope, with the macOS limits above
   (outside the GUI session, sleep). Linux first by hand, then a Mac host.
