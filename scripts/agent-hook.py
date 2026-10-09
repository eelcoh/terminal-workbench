#!/usr/bin/env python3
"""Observe lifecycle events; never produce hook decisions or prompt output."""
import json
import os
from pathlib import Path
import socket
import struct
import sys
import time

LIMIT = 1024 * 1024

def state(event):
    if not isinstance(event, dict) or event.get('agent_id') or event.get('agent_type'):
        return None  # A child must not overwrite the foreground parent's state.
    name = event.get('hook_event_name')
    if name in ('SessionStart', 'Stop', 'Interrupt'):
        return 3
    if name == 'SessionEnd':
        return 0
    if name in ('PermissionRequest', 'Elicitation'):
        return 2
    if name == 'PreToolUse':
        return 2 if event.get('tool_name') in ('AskUserQuestion', 'request_user_input') else 1
    if name in ('UserPromptSubmit', 'PostToolUse', 'PostToolUseFailure', 'ElicitationResult'):
        return 1
    if name == 'Notification':
        kind = event.get('notification_type')
        if kind in ('permission_prompt', 'elicitation_dialog'):
            return 2
        if kind == 'idle_prompt':
            return 3
    return None

def main():
    sequence = time.monotonic_ns()
    try:
        tag = {'codex': 1, 'claude': 2}[sys.argv[1]]
        raw = sys.stdin.buffer.read(LIMIT + 1)
        if len(raw) > LIMIT:
            return
        status = state(json.loads(raw))
        if status is None:
            return
        path = Path(os.environ['WORKBENCH_SESSION_SOCKET']).with_suffix('.ctl')
        group = int(os.environ['WORKBENCH_AGENT_GROUP'])
        token = bytes.fromhex(os.environ['WORKBENCH_AGENT_RUN'])
        if len(token) != 16 or group <= 0:
            return
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(0.4)
            connection.connect(str(path))
            connection.sendall(b'STATE' + struct.pack('!BBi16sQ', tag, status, group, token, sequence))
            connection.recv(16)
    except (OSError, ValueError, KeyError, IndexError, TypeError, OverflowError, RecursionError):
        pass  # Unavailable reporting must not block or alter the agent.

if __name__ == '__main__':
    main()
