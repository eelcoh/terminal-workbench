# GitHub Copilot CLI

Build with `just spike`, then create a new terminal. Run:

```sh
copilot --version
band-copilot
```

Existing terminals retain their running runtime version. Creating a new terminal
obtains the updated runtime and generated launchers; rebuilding the viewer does
not replace existing agent processes. Plain `copilot` tests identity only, with
Unknown lifecycle status. Plain `claude` behaves the same; use `band-claude` for
its hook-driven lifecycle.

`band-copilot` forwards normal arguments and adds a local observer plugin using
`--plugin-dir`. It does not install plugins, edit Copilot settings, enable disabled
hooks or bypass folder trust and approvals. Review normal prompts as usual.
If your version does not support the flag, use plain `copilot` and report the
version. Company hook restrictions must remain in effect.

Startup may remain Unknown until the CLI dispatches its first lifecycle event.
Ask for a simple reply without tools: the header should show Working, then Ready.
Very short transitions may be missed visually.
Ask it to use its built-in question tool to let you choose tea or coffee: check
Needs input in the header and Overview. Answer and check Working/Ready. An actual
permission notification can also report Needs input; a tool attempt alone cannot.
Ready means a reported turn completion, not task success. Missing hooks stay
Unknown; missing later events can leave the last observed status. In the Linux
1.0.95 test, canceling a question returned the CLI to its prompt while Needs input
remained until later lifecycle evidence. Normal answer/exit paths should be checked
separately from cancellation.

Check the agent remains the same process when closing/reopening the viewer or
moving it to another window. Normal `/exit` should clear its label and status.
The owner confirmed native Mac recognition, Working/Ready, Needs input and
return from waiting using the lifecycle trace on 2026-10-14. Normal exit and
remaining edge cases still need manual acceptance; Linux fixtures do not prove them. Report the CLI version, installation method, label
and observed transitions if something differs.

The generated plugin is `.local/integrations/copilot/plugin.json` with its
`hooks.json`. Hooks emit no stdout decisions or prompt content. See
[status semantics](agent-attention.md) and
[GitHub's plugin reference](https://docs.github.com/en/copilot/reference/copilot-cli-reference/cli-plugin-reference).

If an agent is absent from Agents, run `just agent-status` from another terminal
while the agent is running. It reads runtime identity and the viewer's lifecycle
subscription without attaching to PTYs, sending terminal input or restarting
sessions. Save the output for diagnosis. `identity.agent` should be `copilot`;
`viewer_metadata.agent_tag` should be 6. The output also includes runtime PID/start
time and the built binary modification time: a process predating the new build
may still be running older code. A new viewer does not replace that process.
The reported Mac `copilot` executable name and matching PID/group are already
recognized by the current detector; they do not establish which runtime is active.


To repeat the confirmed Mac Working/Ready check, run `just agent-watch 180` from a
second terminal after creating your test Workbench terminal. Then run `band-copilot`
and ask for a no-tool reply, a built-in question, and another reply after answering.
Look for JSON `working`, `ready`, `waiting`, then `working`/`ready` for the Copilot
terminal ID. Move to another workspace while waiting and check Agents/Overview's
Needs input filter. Exit normally and check `agent: null` / `status: unknown`.
Record the exact Copilot version with the trace. It contains no prompt text or
terminal output. Ctrl+C ends only the watcher; it does not stop Copilot or shells.
See [trace limits and options](agent-validation.md#read-only-lifecycle-trace).
