#!/usr/bin/env python3
"""Launch an agent with invocation-local lifecycle observers."""
import json
import os
from pathlib import Path
import shlex
import sys
import uuid

COMMON = ('SessionStart', 'SessionEnd', 'UserPromptSubmit', 'PreToolUse', 'PermissionRequest', 'PostToolUse', 'Stop')

def arguments(agent, tail):
    if agent == 'copilot':
        plugin = Path(__file__).resolve().parent.parent / '.local/integrations/copilot'
        if not (plugin / 'hooks.json').is_file():
            raise ValueError('Copilot observers are missing; rebuild with just spike.')
        return ['copilot', '--plugin-dir', str(plugin), *tail]
    command = shlex.join([sys.executable, str(Path(__file__).with_name('agent-hook.py')), agent])
    events = COMMON + (('Interrupt',) if agent == 'codex' else ('PostToolUseFailure', 'Notification', 'Elicitation', 'ElicitationResult'))
    hooks = {event: [{'hooks': [{'type': 'command', 'command': command, 'timeout': 2}]}] for event in events}
    if agent == 'claude':
        return ['claude', '--settings', json.dumps({'hooks': hooks}), *tail]
    result = ['codex', '--no-daemon']
    # Inline TOML in a session layer is additive to hooks from other layers.
    for event in events:
        value = '[{hooks=[{type="command",command=' + json.dumps(command) + ',timeout=2}]}]'
        result += ['-c', f'hooks.{event}={value}']
    return [*result, *tail]

def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ('codex', 'claude', 'copilot'):
        sys.exit('Usage: workbench-agent.py codex|claude|copilot [agent arguments...]')
    if not os.environ.get('WORKBENCH_SESSION_SOCKET'):
        sys.exit('Run wb-codex, wb-claude or wb-copilot inside a newly created Workbench terminal.')
    environment = dict(os.environ, WORKBENCH_AGENT_GROUP=str(os.getpgrp()), WORKBENCH_AGENT_RUN=uuid.uuid4().hex)
    try:
        args = arguments(sys.argv[1], sys.argv[2:])
    except ValueError as error:
        sys.exit(str(error))
    try:
        os.execvpe(args[0], args, environment)
    except OSError as error:
        sys.exit(f'Could not launch {args[0]}: {error}')

if __name__ == '__main__':
    main()
