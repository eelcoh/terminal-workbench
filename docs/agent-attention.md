# Agent lifecycle status

Run `just spike`, create a new terminal, and use `wb-codex` or `wb-claude` with
normal agent arguments. The generated commands live under .local/bin, added to
PATH only in new Workbench runtime shells. Previously running servers are not
replaced by a build. Create a new terminal to obtain the integration.

The launcher execs the agent in its foreground group and supplies a fresh run
token. Claude receives additional --settings JSON. Codex receives inline session
configuration and --no-daemon so hook processes inherit this terminal's reporting
environment. Existing hooks from other config layers remain applicable. The
launcher does not write agent settings or turn on disabled hooks. Codex may ask
you to review and trust these definitions in /hooks; no trust bypass is used.
Same-event hook overrides supplied by the caller can replace the launcher entry.

| Event | Display |
| --- | --- |
| SessionStart, Stop, Codex Interrupt | Ready |
| UserPromptSubmit, ordinary PreToolUse, PostToolUse | Working |
| PermissionRequest, AskUserQuestion/request_user_input | Needs input |
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

Titles and Overview show the state. The Needs input button filters across all
workspaces; select a row or press Enter to jump to that terminal. Unseen-output
dots and connection labels remain independent.

Validated on Linux using named native-process fixtures, real runtime sockets,
hook payloads and isolated GUI tests. Launcher argument capture verifies generated
settings and environment. Real interactive Codex/Claude releases and macOS hook
execution remain unverified; supported schemas vary by agent version. A container
agent that cannot reach the host socket or inherit its environment will not report.

Schemas: [Codex hooks](https://developers.openai.com/codex/hooks/) and
[Claude Code hooks](https://code.claude.com/docs/en/hooks).
