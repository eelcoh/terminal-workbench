# Agent lifecycle status

Run `just spike`, create a new terminal, and use `wb-codex`, `wb-claude` or `wb-copilot` with
normal agent arguments. The generated commands live under .local/bin, added to
PATH only in new Workbench runtime shells. Previously running servers are not
replaced by a build. Create a new terminal to obtain the integration.

Plain `claude`, `codex` or `copilot` launches provide identity only and remain Unknown for
lifecycle status. Linux validation also ran the real Workbench runtime and Claude
2.1.296 inside build-machine: SessionStart, prompt submission and completed-turn
hooks reported Ready/Working/Ready. This does not prove a host runtime can identify
an agent behind a container wrapper; the runtime, foreground group and reporting
socket must be reachable in the agent's environment. The launcher adds observers
for that invocation without editing the user's agent hook definitions.

The launcher execs the agent in its foreground group and supplies a fresh run
token. Claude receives additional --settings JSON. Codex receives inline session
configuration and --no-daemon so hook processes inherit this terminal's reporting
environment. Existing hooks from other config layers remain applicable. The
launcher does not write agent settings or turn on disabled hooks. Codex may ask
you to review and trust these definitions in /hooks; no trust bypass is used.
Same-event hook overrides supplied by the caller can replace the launcher entry.

Copilot receives an invocation-local `--plugin-dir` containing command observers.
It does not install a plugin or edit user/project settings. Its camelCase events
are normalized explicitly; `agentStop` reports Ready and `ask_user` reports Needs
input. Actual permission/elicitation notifications report Needs input. Copilot's
`permissionRequest` occurs before its rules engine and is deliberately not used
as evidence of a displayed prompt. Background-agent notifications are ignored;
`errorOccurred` clears status to Unknown. Older versions lacking `--plugin-dir`
can use plain `copilot` with identity only. See [Copilot testing](copilot.md).

| Event | Display |
| --- | --- |
| SessionStart, Stop, Codex Interrupt | Ready |
| UserPromptSubmit, ordinary PreToolUse, PostToolUse | Working |
| Codex/Claude PermissionRequest, AskUserQuestion/request_user_input | Needs input |
| Claude Elicitation or permission/elicitation notification | Needs input |
| Claude ElicitationResult, PostToolUseFailure | Working |
| Claude idle notification | Ready |
| SessionEnd, foreground agent exit/change | Unknown |

Ready means the agent reported a completed turn or started a session. It does
not mean a task succeeded. Working and Needs input are the last observed events,
not screen detection. Agents launched normally, disabled/untrusted hooks, remote
agents and unsupported event types remain Unknown. Lost events can leave the
last reported state until the next event or foreground change. There is no
heartbeat freshness guarantee. Main-session observers ignore payloads with
agent_id/agent_type and do not subscribe to subagent lifecycle events. A Stop
hook that asks the agent to continue can briefly report Ready until another
supported event arrives.

The observer reads bounded JSON and sends only numeric identity/status, group,
run token, and monotonic start sequence through the local control endpoint.
It emits no stdout, makes no approval decisions, and silently returns success
when reporting is unavailable. Reports with a wrong group/token or older sequence
are rejected. Hook start order is used for concurrent callbacks; this is not a
global event clock. Metadata survives viewer detach/reconnect; it is not persisted
as organization or appended to terminal history.

The terminal header and each Overview entry show that terminal's state. The
Agents rail view also lists observed agents across workspaces with terminal
context, All agents/Needs input filters, and a count of agents needing input.
Spatial cards show it at the bottom, for example `Copilot / Ready`; list rows show
it at the right. The Overview button switches between **All states** and
**Needs input**. These are filter choices, not the focused agent's status: the
button is always available, including when no agent needs input. Needs input
filters across all workspaces; select a row or press Enter to jump to that
terminal. Unseen-output dots and connection labels remain independent.

Validated on Linux using named native-process fixtures, real runtime sockets,
hook payloads and isolated GUI tests. Launcher argument capture verifies generated
settings and environment. Real Linux Codex 0.161.0 and Claude Code 2.1.296 sessions now validate
foreground identity, hook-driven Working/Ready, question-driven Needs input and
same-process detach/reattach. Real Linux permission prompts, background
Agents/Overview filters and accepted commands now pass with Claude 2.1.299 and Codex 0.162.1. Claude denial can interrupt
without a completion event and retain Needs input until the next hook, as can
Copilot question cancellation. Native Mac full lifecycle acceptance remains open;
supported schemas vary by agent version. See
[the live validation record](agent-validation.md). A container
agent that cannot reach the host socket or inherit its environment will not report.

Schemas: [Codex hooks](https://developers.openai.com/codex/hooks/) and
[Claude Code hooks](https://code.claude.com/docs/en/hooks) and
[Copilot hooks](https://docs.github.com/en/copilot/reference/hooks-reference).

Use `just agent-watch` to trace delivered identity/status changes without taking
terminal ownership. See [live review and trace usage](agent-validation.md#read-only-lifecycle-trace).
