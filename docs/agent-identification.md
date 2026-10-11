# Foreground agent identification

The runtime samples its PTY foreground process-group leader once per second.
Recognition uses executable filenames (codex, claude, gemini, opencode, aider, copilot)
and selected Node/Python launcher paths. The native Claude install layout also
recognizes numeric version filenames beneath .local/share/claude/versions. It never scans terminal output or user
prompts, and never classifies an agent as working, idle, or waiting from activity.
Executable names are a heuristic, not authenticated application identities.

Linux reads the leader's executable path from /proc and a bounded launcher prefix
when the executable is Node/Python. It does not scan all processes or recurse into
descendants. Restricted/unavailable process information produces Unknown; an
unrecognized readable executable produces no recognized agent. Names of background
agents, container wrappers, tmux sessions, custom launchers and wrappers that do
not retain a recognized leader can be missed. A foreground group does not have to
represent a coding agent, even if its executable uses a recognized name.

macOS has a separate adapter using /bin/ps to obtain only the leader's executable
path. For Node/Python leaders it reads a fixed 16 KiB process-argument buffer
and decodes only the executable and first launcher argument. This covers the
Copilot npm-loader.js layout without exposing prompts or environment through the
API. Oversized or unavailable arguments leave script recognition unavailable.
Native macOS agent recognition remains unverified. Other Unix targets return
unavailable. No system process arguments or prompts are exposed by the API.

The JSON snapshot is available through `workbench-session inspect SOCKET`:

```json
{"agent":"codex","available":true,"source":"foreground-process","status":"unknown","status_source":"unavailable"}
```

Null agent with available=true means no recognized foreground agent; available=false
means identification is unavailable. The snapshot is sampled, so a short-lived
process may never be observed. Metadata is runtime-owned and continues updating
without an attached viewer. It is not saved as workspace metadata or output history.

The viewer's optional control subscription receives current identity and updates.
It adds agent labels to pane titles/overview rows and an Agents filter. Missing or
rejected metadata subscriptions leave normal terminal input/output intact. Older
live servers remain usable, without labels; exit those shells and reopen to start
new servers. Dropping the foreground agent clears its label on the next sample.
Disconnected panes are excluded from the Agents filter.

`just check` uses disposable executables with recognized names to prove foreground
lookup, subscribe/inspect, detach/reattach, clearing, ordinary-output isolation and
watcher cleanup. Unit tests exercise launcher boundaries and unknown evidence.
The GUI test uses a named sleep fixture to verify labels, filtering and clearing.
These checks prove our implementation contracts, not compatibility with current
interactive releases of all six agents. Live CLI/version validation remains open.

Launcher references inspected for this experiment:

- [Codex package entry](https://github.com/openai/codex/blob/main/codex-cli/package.json)
- [Gemini CLI package entry](https://github.com/google-gemini/gemini-cli/blob/main/packages/cli/package.json)

The next status layer needs explicit integration signals or server-owned terminal
state with evidence-based detection; it must not reinterpret output dots as status.

On this workstation, read-only installation inspection found Codex 0.161.0 with
an executable named codex, and Claude 2.1.289 behind the ~/.local/bin/claude symlink
to ~/.local/share/claude/versions/2.1.289. The latter required a specific install-path
rule because the resolved executable basename is its version. Both launch layouts
are covered. Subsequent real Linux validation exercised Codex 0.161.0 and
Claude Code 2.1.296; see [the live validation record](agent-validation.md).
Other agents and native macOS recognition remain unverified.

Lifecycle status is separate, optional hook evidence. See [hook integration](agent-attention.md).
