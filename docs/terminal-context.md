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
target/debug/workbench-session context .local/sessions/TERMINAL_ID.sock
```

The JSON reports `available`, `source`, `cwd`, `repository`, `branch`, `worktree`
and `detached`. A missing/unsupported runtime returns an error. Unavailable local
process metadata has empty context; remote clients report `remote-unavailable`.

## Runtime and compatibility

Linux uses `/proc/PID/cwd`; Mac uses `proc_pidinfo`. The existing foreground
process sample supplies the target PID. A runtime worker reads bounded Git
`HEAD`, gitfile and `commondir` metadata, caches repository discovery for two
seconds, and publishes only changed values. It never launches Git or walks
process descendants. Ordinary checkouts, linked worktrees, submodules and unborn
branches are supported. Bare repositories and shell-specific `GIT_DIR` overrides
are not inferred. Unreadable/malformed metadata falls back to the directory.

The optional `terminal-context` capability exposes `CWD1` JSON reads and `CTX1`
subscriptions on the private control endpoint. Subscription frames use kind 10,
payload version 1, availability/detached/remote flag bits, then four UTF-8 strings
(cwd, repository, branch, worktree), each with a big-endian u16 byte length.
Payloads are bounded to 8192 bytes. Separate bounded queues isolate slow readers;
context errors never disconnect the PTY or agent watch. Existing INFO, agent
watch, snapshot and PTY codecs remain unchanged.

Custom names use optional JSON `custom_name` metadata and a companion
`terminal-labels.tsv` projection indexed by stable terminal ID. Existing catalog
formats remain unchanged. Legacy generic `Shell N`/`Terminal N` names are automatic;
other names are overrides. Explicit Rename preserves even a generic name.
Missing/corrupt optional projection data uses this legacy inference.

`just check` covers live process cwd, Git transitions, detached independent
reads, protocol validation and catalog compatibility. `just context-gui-test`
checks live search/navigation and captures both Overview layouts on Linux.
