# Live terminal context in Overview

New terminals show their repository and branch in Overview. A linked Git worktree
adds its checkout name, and a detached HEAD shows its short commit hash. The
current directory appears underneath. Outside Git, the directory is the title.
Pane headers use the same live repository/branch/worktree context. Outside Git,
they show the folder name to keep the header compact; Overview retains the full
path. Pane-position tooltips follow these labels too. Long headers are ellipsized
without displacing agent status or pane controls.
An explicitly renamed terminal keeps its title; its current directory remains
visible. Search matches the context as well as workspace, custom name and agent.

Context follows the local foreground process, including shell `cd` and branch
changes made elsewhere. It refreshes about once a second, including hidden panes
and terminals owned by another window. It never changes shell ownership, input,
terminal size or agent attention state. Remote SSH/mosh sessions retain their
saved name: the local client's directory is not presented as the remote folder.

After rebuilding, create a **new terminal** to try this. Existing runtimes keep
running their old code and retain their saved names; they are never restarted to
enable context. Native Mac testing of this slice remains pending.

To inspect a terminal without attaching or moving it:

```sh
target/debug/band-session context .local/sessions/TERMINAL_ID.sock
```

The JSON reports `available`, `source`, `cwd`, `repository`, `branch`, `worktree`
and `detached`. A missing/unsupported runtime returns an error. Unavailable local
process metadata has empty context; remote clients report `remote-unavailable`.

## Runtime and compatibility

Linux uses `/proc/PID/cwd`; Mac uses `proc_pidinfo`. The existing foreground
process sample supplies the target PID. A runtime worker reads bounded Git
`HEAD`, gitfile and `commondir` metadata, caches repository discovery for two
seconds, and publishes only changed values. It does not walk process
descendants. Branch and worktree come from those files; only uncommitted
changes start Git (below). Ordinary checkouts, linked worktrees, submodules and unborn
branches are supported. Bare repositories and shell-specific `GIT_DIR` overrides
are not inferred. Unreadable/malformed metadata falls back to the directory.

The optional `terminal-context` capability exposes `CWD1` JSON reads and `CTX1`
subscriptions on the private control endpoint. Subscription frames use kind 10,
payload version 1, availability/detached/remote flag bits, then four UTF-8 strings
(cwd, repository, branch, worktree), each with a big-endian u16 byte length.
Payloads are bounded to 8192 bytes. Separate bounded queues isolate slow readers;
context errors never disconnect the PTY or agent watch. Existing INFO, agent
watch, snapshot and PTY codecs remain unchanged.

## Uncommitted changes (CTX2)

`CTX2` subscriptions (capability `terminal-context-v2`) carry the same payload
with two more flag bits: bit 3, dirty state known, and bit 4, uncommitted
changes (including untracked files). `CTX1` subscribers keep the three original
bits and receive no frames for dirty-only changes. The viewer asks for CTX2 and
falls back to CTX1 when an older runtime closes the handshake. `CWD1` JSON adds
`"dirty": true`, `false` or `null`.

The context worker runs `git status --porcelain` in the pane's worktree on a
change of worktree and then at most every five seconds, backing off to twenty
times the time the last run took (up to two minutes). Each run has a two-second
limit and is killed after it; Git missing, a refused repository (such as
`safe.directory`) or a timeout leave the state unknown. It runs with
`core.fsmonitor=false`, `--no-optional-locks` and no terminal prompt, so the
check takes no index lock and a repository's fsmonitor hook does not run.
Clean or smudge filters configured by the user can still run, as with any
`git status`.

Two things bring the next check forward:
- the worktree's index file changes, which `git add`, `commit`, `checkout`,
  `reset` and `stash` all do;
- the pane's foreground job starts or ends, so a command just run is checked.

An early check still waits until four times the last run's duration has passed,
and at least a second, so a slow repository is not checked in a loop.

Panes in one worktree share the result through `<state>/git-status/`, a private
folder with one small file per worktree (named by a hash of its root, with the
root inside to rule out a collision). A runtime reuses a result that started
after the index last changed and after its own trigger, within that result's
interval. Otherwise it takes the worktree's lock and runs Git itself; a runtime
that finds the lock taken waits for the next second rather than run Git
alongside. Files for worktrees no longer visited stay until the state folder
is removed.

Custom names use optional JSON `custom_name` metadata and a companion
`terminal-labels.tsv` projection indexed by stable terminal ID. Existing catalog
formats remain unchanged. Legacy generic `Shell N`/`Terminal N` names are automatic;
other names are overrides. Explicit Rename preserves even a generic name.
Missing/corrupt optional projection data uses this legacy inference.

`just check` covers live process cwd, Git transitions, detached independent
reads, protocol validation and catalog compatibility. `just context-gui-test`
checks live search/navigation and captures both Overview layouts on Linux.
