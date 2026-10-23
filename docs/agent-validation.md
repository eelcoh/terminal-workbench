# Real agent validation

## Platform matrix

The owner has Claude Code and Codex on Linux. GitHub Copilot CLI is the only
permitted agent on the Mac; Mac acceptance must use Copilot, without requiring
Claude or Codex installation there. Additional Linux candidates include Pi,
Gemini CLI, OpenCode and Hermes. Installation availability is not compatibility
evidence. They are follow-up targets, not prerequisites for the first slice.

Copilot now has native/npm foreground identification and an optional `band-copilot`
invocation-local plugin for lifecycle events. Runtime/socket and GUI fixtures
validate the integration contracts; complete native macOS lifecycle acceptance
remains pending.
Ordinary input, trust and permissions are retained, with Unknown when lifecycle
signals are unavailable. Record the installed CLI version and use
the [official hooks reference](https://docs.github.com/en/copilot/reference/hooks-reference)
to establish the supported events; do not assume Claude's payload or configuration
shape applies. Company policies may restrict hooks as well as CLI installation.

## Linux observations — 2026-10-11

Runtime tested: `8359202`, using disposable empty directories and real installed
CLI executables. Launchers are invocation-local observers built into `band-session`.
The agent ran interactively in a Workbench PTY; the test attached a protocol
client and inspected runtime metadata and retained terminal text. This is live
CLI/runtime evidence, not graphical or native input acceptance.

| Check | Codex 0.161.0 | Claude Code 2.1.296 |
| --- | --- | --- |
| Foreground identification | Pass | Pass |
| Untrusted startup | Hook-review screen, Unknown | Folder-trust screen, Unknown |
| Completed no-tool turn | Working → Ready; expected reply | Ready → Working → Ready; expected reply |
| Built-in question | Needs input, plan mode | Needs input, AskUserQuestion |
| Detach/reattach while ready | Same PID and metadata | Same PID and metadata |
| Normal foreground exit | Identity clears, status Unknown | Identity clears, status Unknown |
| Permission request | Pending | Pending |
| Graphical background workspace and Needs input filter | Pending live CLI check | Pending live CLI check |

Codex observers were reviewed and accepted through its normal hook-trust UI;
no trust-bypass flag was used. Hooks remain subject to agent policy and disabled
settings. Codex's input screen can appear before the first SessionStart event;
Unknown at that point must not be interpreted as a broken integration. The
installed version's update prompt was skipped; no CLI upgrade was performed.
Claude's empty disposable workspace was explicitly trusted. Existing account
settings and hooks were retained. The probes requested a no-tool echo and a
tea/coffee question, without project edits or external messages.

The tests observed live hook transitions, preserved the agent process across
client detach and cleaned up only their disposable runtime sessions. Temporary
diagnostics recorded event names/field names and reporting-environment presence,
without prompt bodies, transcripts or credentials. Identity and lifecycle remain
distinct: a recognized foreground executable does not establish Working or Ready.

The same installed Claude release also passed a no-tool Ready/Working/Ready turn
with both the Workbench runtime and CLI launched inside build-machine. This
validates that shared container environment, not arbitrary host/container
boundaries or descendant-process discovery. Plain Claude remains identity-only.

## Copilot Linux observations — 2026-10-11

The current Copilot integration was tested with the real native Linux binary
from `@github/copilot` 1.0.95 in a disposable Workbench PTY. The empty test folder
was trusted for that session only. A no-tool echo produced Working → Ready and
the expected reply. Detach/reattach retained the same live PID and metadata.
The built-in `ask_user` question produced Needs input; answering returned
Working → Ready. Normal `/exit` cleared identity and status. Startup remained Unknown
until the first prompt dispatched lifecycle events.

Canceling the question through Ctrl+C returned the CLI to its prompt but emitted
no observed completion report; Workbench retained Needs input. This is a known
missing-event limitation, not permission to infer status from output or enable
hooks against policy. Interrupt/resume paths remain an open validation item.

`just check` and `just copilot-gui-test` pass. GUI checks use a named executable
fixture and supplied hook events, not a live CLI; live permission prompts and
graphical/background acceptance remain pending. The owner reports Copilot
**1.0.96-2** on macOS; native identity is now confirmed and lifecycle acceptance
remains pending. The Mac adapter also recognizes the npm Node launcher layout through
a bounded process-argument prefix, with decoder tests on Linux.

## macOS identification — 2026-10-11

The owner confirmed Copilot appears under Agents when launched in a terminal
whose runtime was created after the Copilot build. Native Homebrew Copilot
1.0.96-2 had a `copilot` executable name and a foreground group matching its PID.
Read-only diagnostics recorded `identity.agent: copilot` and viewer metadata
`agent_tag: 6` for the same running runtime, PID 33412. Its snapshot still reported
Unknown lifecycle status, so this confirms identity and delivery to the viewer,
not Working/Ready or Needs input hooks on macOS. In subsequent manual testing,
the owner confirmed the Needs input filter retains Copilot while it is waiting
and is empty otherwise. This validates native waiting-state delivery and
filtering. Working/Ready and return from waiting were subsequently confirmed by
the 2026-10-14 trace below; normal exit and the remaining native edge cases are
still separate checks.

Older saved terminals retain their running code after rebuilding the viewer.
Creating a new terminal obtains the new runtime; restarting an agent inside an
older terminal does not update that runtime. The owner separately confirmed
plain Claude and Codex labels on Linux; their labels do not require `wb-*`.

## Manual acceptance sequence

1. Build with `just spike`; create a new terminal so it has current launchers.
2. Run `band-codex`, `band-claude` or `band-copilot` in an empty test workspace. Review normal trust
   prompts; check Unknown before lifecycle evidence arrives.
3. Ask for a simple no-tool reply. Check Working while processing and Ready
   after the turn completes. Ready does not claim the task succeeded.
4. Ask for a built-in multiple-choice question (plan mode in Codex). Check Needs
   input in the header and overview; change workspaces and use its filter to return.
5. Answer the question and check Working/Ready. Separately provoke a harmless
   permission request with normal approval settings and test both accept and deny.
6. Close/reopen the viewer while leaving the agent running. Check the same
   process and current status, then repeat with a second window and Move here.
7. Exit the agent normally. Check identity and status clear after the runtime
   foreground sample; ordinary shell output must not create agent status.
8. Test disabled or untrusted hooks: ordinary input still works, lifecycle stays
   Unknown. Do not enable hooks against user or administrator policy.

Mac follow-up uses `band-copilot` ([instructions](copilot.md)), records its exact
version and repeats the relevant sequence on native macOS. Permission behavior,
resume/interrupt/error paths, child-agent isolation, stale/missing signals and
native input/GUI behavior remain part of the milestone; the table above does not
close the release gate.

## Linux permission and background-workspace review — 2026-10-14

Runtime/viewer base: `b85c0bb`. Real Claude Code **2.1.299** and Codex **0.162.1**
ran inside build-machine in disposable directories, with the actual Workbench GUI
on isolated Xvfb displays. This establishes real CLI/hook/GUI behavior on Linux
X11/software rendering; physical Wayland, Mac and other versions remain separate.

Claude used default/manual permissions with an invocation-only `ask: ["Bash"]`
rule merged into the observer settings. Codex used on-request approvals and a
read-only sandbox; the probe explicitly requested approval for the harmless shell
command `printf WB_PERMISSION_PROBE`. Claude's empty directory was trusted through
the ordinary folder UI; Codex reused already-reviewed observer definitions.
No trust-bypass flag, persistent permission rule or automated hook decision was
used. Existing agent account/settings layers stayed applicable. Agent approval was
granted once through each CLI's normal prompt; denial was tested separately.

| Check | Claude 2.1.299 | Codex 0.162.1 |
| --- | --- | --- |
| Permission prompt | Working → Needs input | Working → Needs input |
| Background Agents drawer/count | Waiting agent visible from another workspace | Waiting agent visible from another workspace |
| Overview Agents + Needs input | Only waiting agent included | Only waiting agent included |
| Jump back from overview | Same pending approval, no accidental acceptance | Same pending approval, no accidental acceptance |
| Approve once | Working → Ready, command output | Working → Ready, command output |
| Ready agent excluded by Needs input | Pass | Pass |
| Deny/interrupt | Prompt canceled; last Needs input retained | Ready through Interrupt |
| Follow-up no-tool turn | Working → Ready clears stale state | Working → Ready |
| Normal `/exit` | Foreground identity/status clear | Foreground identity/status clear |

Claude's denial interrupted the turn without an observed completion event. The
observer cannot honestly infer Ready from screen text or a timeout; Needs input
can remain until the next lifecycle event or foreground exit. This joins the
previous Copilot cancellation limitation. Cancellation/event completeness remains
an open gate. No observer/runtime behavior was changed to hide it.

Private local screenshots under `.local/artifacts/agent-live/2026-10-14/` record
waiting agents in background drawers/overview and empty waiting filters after
completion. They are acceptance artifacts, not committed CLI-screen fixtures.

## Read-only lifecycle trace

Run `just agent-watch` in another terminal, then interact with a real agent in
Workbench. The default observation is two minutes; `just agent-watch 300` records
five minutes. Ctrl+C stops watching. Output is newline-delimited JSON with UTC
timestamps, stable workspace/terminal IDs, agent identity and delivered status
changes (`unknown`, `working`, `waiting`, `ready`). `waiting` is the UI's Needs input.
Repeated identical states are omitted. The command captures runtime-delivered
transitions, not every hook callback or a guarantee that events were emitted.

The watcher reads the existing catalog once, probes bounded read-only runtime
capabilities and selects `WAT3` when advertised, otherwise baseline `WAT2`, on
user-owned control sockets. It never attaches a PTY, sends terminal input, starts
services or agents, recovers terminals, or writes preferences/catalogs. It does
not read terminal output, prompts, transcripts or credentials. Missing/stopped,
unsupported or disconnected endpoints emit `unavailable` independently; a closed
metadata connection is not a claim that its shell exited. No automatic reconnect
or catalog refresh occurs: create terminals before starting the watcher, and
restart it after adding terminals or restarting runtimes. Legacy endpoints may
lack the subscription and retain normal terminal use.

`BAND_STATE_DIR` selects an isolated/custom session directory. Direct use is
`python3 scripts/agent_watch.py --state-dir PATH --seconds 120`. Durations are
bounded to 1–3600 seconds; the validated catalog allows at most 64 subscriptions,
with four bounded connection attempts at a time and fixed-size metadata frames.

`just check` covers real runtime events alongside an attached/usable terminal,
unchanged catalogs, failed-sibling isolation, fragmented/coalesced frames,
deduplication, EOF, malformed lengths/tags, unsafe sockets and duration validation.
A live Codex no-tool turn also delivered Ready → Working → Ready through the
watcher while the GUI retained terminal ownership. The owner also confirmed
native Mac Copilot tracing on 2026-10-14, as recorded below.


Rapid synthetic `xdotool type --delay 12` input in these live interactive CLIs
produced garbled/duplicated prompt characters, including malformed slash commands.
Paced individual key presses avoid that observation. This is an open input
acceptance finding, not proof of physical keyboard failure or its root cause.
Add a raw-input reproduction and compare legacy/Kitty modes before changing the
adapter. Permission command names/output and lifecycle metadata were checked
directly; transcript spelling alone was not used as compatibility evidence.


## macOS Copilot lifecycle trace — 2026-10-14

The owner confirmed `band-copilot` and `just agent-watch 180` on native Mac.
The supplied trace shows a single terminal changing from ordinary shell/Unknown
to Copilot/Unknown, then Working → Ready → Needs input → Working → Ready.
The first Working interval lasted about 67 ms, so it can be hard to see in the
header; the later Working interval lasted about 1.59 seconds. This confirms
native lifecycle hook delivery, the runtime subscription, and return from waiting.
It does not establish a specific permission/question source because the trace
deliberately contains no prompt or tool content.

The earlier all-Unknown result came from plain `copilot`, which the owner
confirmed had been launched without the Workbench observer wrapper. No code fix
was needed. Existing Mac Needs input filter acceptance remains recorded above.
The latest reported Mac version is 1.0.96-2; this trace did not include a fresh
version check. Normal exit, cancellation, permission accept/deny, disabled-hook
policy, and full background/handoff edge cases remain follow-up native checks.
The user-owned debug files are retained locally and are not committed.

The rapid synthetic-input finding above is now reproduced and fixed locally on
Linux with ordered native events and batched Ghostty input. See
[terminal input](terminal-input.md) for the regression and remaining native gates.

## Pi on Linux — 2026-10-21

Pi **1.3.0**, installed under `~/.pi/agent/install/releases/1.3.0`, passed a
real no-tool turn through a disposable Workbench runtime based on `325afda` with
this batch's changes. The invocation-local `band-pi` observer produced Unknown →
Working → Ready and the expected reply; foreground exit cleared identity.
The test used an ephemeral session, disabled tools/MCP and automatic extension/
context discovery, and explicitly loaded only the Workbench observer. It did not
change project/user settings. Private evidence is under
`.local/artifacts/pi-live/`.

This installed Pi rewrites Node argv to the exact process title `pi`. Recognition
uses the Node executable plus that exact invocation when no script remains, or
known package launcher paths. Prompt/output text and process-tree searches do
not establish identity. Plain `pi` can be recognized independently of lifecycle
observers; older saved runtimes still require a newly created terminal for Pi
recognition. Rebuilding a viewer does not upgrade a retained runtime.

`band-pi` adds one explicit `--extension` file for this invocation. The observer
reports Working on `agent_start`, Ready on `agent_settled`, and Unknown on
`session_shutdown`. It creates no resources at extension factory time, returns no
decisions, changes no tools/permissions, and reads no prompt/tool/transcript data.
It uses the existing bounded STATE report with group/run/sequence validation and
checks its own PID against the launcher's PID so child Pi processes cannot reuse
the parent reporting identity. Reporting failure remains silent.

Pi's [extension lifecycle documentation](https://pi.dev/docs/latest/extensions)
explains that `agent_end` can precede retries/continuations; final idle uses
`agent_settled`. Needs input has no verified observer in this slice: do not infer
it from arbitrary tool starts or output. Cancellation, resumed sessions,
permission prompts and Mac Pi behavior remain separate acceptance work. Mac
acceptance continues to use the owner's permitted GitHub Copilot CLI.

Pi is exposed through the separately named `WAT3` metadata subscription. Existing
WATCH/WAT2 clients keep their original tag range and see Unknown for Pi; Codex,
Claude and other existing tags retain their bytes. Qt retries WAT2 once only when
an old runtime closes the new request before any response, never after a partial
or malformed reply. CLI tracing selects WAT3 by advertised capability. These are
read-only subscriptions and never attach, recover, transfer or send PTY input.

To review on Linux, install/open the current preview and create a new terminal.
Run plain `pi` to check identity-only recognition, or `band-pi` for lifecycle reports.
After a no-tool reply, check Working/Ready in its header, Agents and Agent details.
If the wrapper is missing, the shell still has older launch helpers. Normal trust
and policy settings remain in effect; no bypass is required. Use `just agent-watch`
with the matching session directory for a content-free trace.

The owner's first `band-pi` trial on 2026-10-21 used a retained terminal whose shell
and runtime started on 10 October. Read-only inspection confirmed that runtime
had neither the new metadata endpoint nor Pi identity support. Starting Pi today
inside a retained pane does not upgrade the runtime. Use New terminal
(Super+Alt+T on Linux), then launch `band-pi`; existing work can stay running.
Qt/runtime-only builds now prepare invocation-local helpers too, removing the
previous dependency on a full spike build or manual helper generation.
