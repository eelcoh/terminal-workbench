# Real agent validation

## Platform matrix

The owner has Claude Code and Codex on Linux. GitHub Copilot CLI is the only
permitted agent on the Mac; Mac acceptance must use Copilot, without requiring
Claude or Codex installation there. Additional Linux candidates include Pi,
Gemini CLI, OpenCode and Hermes. Installation availability is not compatibility
evidence. They are follow-up targets, not prerequisites for the first slice.

Copilot currently has no Workbench identity tag, launcher or lifecycle adapter.
Its integration must retain ordinary input, trust and permission behavior, and
remain Unknown where signals are unavailable. Use the installed CLI version and
the [official hooks reference](https://docs.github.com/en/copilot/reference/hooks-reference)
to establish the supported events; do not assume Claude's payload or configuration
shape applies. Company policies may restrict hooks as well as CLI installation.

## Linux observations — 2026-10-11

Runtime tested: `8359202`, using disposable empty directories and real installed
CLI executables. Launchers are invocation-local `workbench-agent.py` observers.
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

## Manual acceptance sequence

1. Build with `just spike`; create a new terminal so it has current launchers.
2. Run `wb-codex` or `wb-claude` in an empty test workspace. Review normal trust
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

Mac follow-up uses Copilot after its adapter is implemented, records its exact
version and repeats the relevant sequence on native macOS. Permission behavior,
resume/interrupt/error paths, child-agent isolation, stale/missing signals and
native input/GUI behavior remain part of the milestone; the table above does not
close the release gate.
