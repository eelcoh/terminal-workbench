#!/usr/bin/env python3
"""Generate local PATH entries without changing shell or agent configuration."""
from pathlib import Path
import json
import shlex

root = Path(__file__).resolve().parent.parent
directory = root / '.local/bin'
directory.mkdir(parents=True, exist_ok=True)
for agent in ('codex', 'claude', 'copilot'):
    path = directory / f'wb-{agent}'
    contents = '#!/bin/sh\nexec python3 ' + shlex.quote(str(root / 'scripts/workbench-agent.py')) + ' ' + agent + ' "$@"\n'
    if not path.exists() or path.read_text() != contents:
        path.write_text(contents)
    path.chmod(0o755)

# Copilot supports invocation-local plugin directories. No files are installed
# into project/user settings; policy, trust and other plugins remain in effect.
plugin = root / '.local/integrations/copilot'
plugin.mkdir(parents=True, exist_ok=True)
manifest = {'name': 'workbench-observers', 'version': '0.1.0', 'license': 'MIT',
    'description': 'Optional local Workbench lifecycle status observers', 'hooks': 'hooks.json'}
events = ('sessionStart', 'sessionEnd', 'userPromptSubmitted', 'preToolUse',
    'postToolUse', 'postToolUseFailure', 'agentStop', 'errorOccurred', 'notification')
hooks = {'version': 1, 'hooks': {event: [{'type': 'command',
    'bash': shlex.join(['python3', str(root/'scripts/agent-hook.py'), 'copilot', event]),
    'timeoutSec': 2}] for event in events}}
for name, value in [('plugin.json', manifest), ('hooks.json', hooks)]:
    path = plugin / name
    contents = json.dumps(value, indent=2) + '\n'
    if not path.exists() or path.read_text() != contents:
        path.write_text(contents)
