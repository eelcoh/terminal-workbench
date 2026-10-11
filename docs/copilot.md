# GitHub Copilot CLI

Build with `just spike`, then create a new terminal. Run:

```sh
copilot --version
wb-copilot
```

Existing terminals retain their running runtime version. Creating a new terminal
obtains the updated runtime and generated launchers; rebuilding the viewer does
not replace existing agent processes. Plain `copilot` tests identity only, with
Unknown lifecycle status. Plain `claude` behaves the same; use `wb-claude` for
its hook-driven lifecycle.

`wb-copilot` forwards normal arguments and adds a local observer plugin using
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
macOS native recognition and hook execution need this manual acceptance; Linux
fixtures do not prove them. Report the CLI version, installation method, label
and observed transitions if something differs.

The generated plugin is `.local/integrations/copilot/plugin.json` with its
`hooks.json`. Hooks emit no stdout decisions or prompt content. See
[status semantics](agent-attention.md) and
[GitHub's plugin reference](https://docs.github.com/en/copilot/reference/copilot-cli-reference/cli-plugin-reference).
