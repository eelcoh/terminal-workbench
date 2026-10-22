#!/usr/bin/env python3
"""Generate local PATH entries without changing shell or agent configuration."""
from pathlib import Path
import json
import os

root = Path(__file__).resolve().parent.parent
directory = root / '.local/bin'
directory.mkdir(parents=True, exist_ok=True)
# The runtime binary acts as each launcher and the hook observer, selected by
# the name it is invoked through. The link resolves once cargo has built it.
target = os.path.relpath(root / 'target/debug/workbench-session', directory)
for name in ('wb-codex', 'wb-claude', 'wb-copilot', 'wb-pi', 'wb-agent-hook'):
    path = directory / name
    if path.is_symlink() and os.readlink(path) == target:
        continue
    if path.exists() or path.is_symlink():
        path.unlink()
    path.symlink_to(target)

# Copilot supports invocation-local plugin directories. No files are installed
# into project/user settings; policy, trust and other plugins remain in effect.
# Hooks run inside a Workbench shell, where the runtime sets WORKBENCH_AGENT_BIN.
plugin = root / '.local/integrations/copilot'
plugin.mkdir(parents=True, exist_ok=True)
manifest = {'name': 'workbench-observers', 'version': '0.1.0', 'license': 'MIT',
    'description': 'Optional local Workbench lifecycle status observers', 'hooks': 'hooks.json'}
events = ('sessionStart', 'sessionEnd', 'userPromptSubmitted', 'preToolUse',
    'postToolUse', 'postToolUseFailure', 'agentStop', 'errorOccurred', 'notification')
hooks = {'version': 1, 'hooks': {event: [{'type': 'command',
    'bash': '"$WORKBENCH_AGENT_BIN/wb-agent-hook" copilot ' + event,
    'timeoutSec': 2}] for event in events}}
for name, value in [('plugin.json', manifest), ('hooks.json', hooks)]:
    path = plugin / name
    contents = json.dumps(value, indent=2) + '\n'
    if not path.exists() or path.read_text() != contents:
        path.write_text(contents)

# The Pi extension is loaded only for wb-pi, with no settings installation.
pi = root / '.local/integrations/pi'
pi.mkdir(parents=True, exist_ok=True)
(pi / 'workbench.ts').write_bytes((root / 'integrations/pi/workbench.ts').read_bytes())
