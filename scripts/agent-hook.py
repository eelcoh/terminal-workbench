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

COPILOT_EVENTS = {'sessionStart': 'SessionStart', 'sessionEnd': 'SessionEnd',
    'userPromptSubmitted': 'UserPromptSubmit', 'preToolUse': 'PreToolUse',
    'postToolUse': 'PostToolUse', 'postToolUseFailure': 'PostToolUseFailure',
    'agentStop': 'Stop', 'errorOccurred': 'ErrorOccurred', 'notification': 'Notification'}

def copilot_state(event, configured_event):
    if not isinstance(event, dict) or configured_event not in COPILOT_EVENTS:
        return None
    if any(event.get(field) for field in ('agent_id', 'agent_type', 'agentId', 'agentType', 'agentName', 'parentSessionId')):
        return None
    name = COPILOT_EVENTS[configured_event]
    supplied_name = event.get('hook_event_name')
    if supplied_name is not None and supplied_name not in (name, configured_event):
        return None
    # PermissionRequest is before the rules engine, not proof of a prompt.
    # Only actual notifications or the question tool establish Needs input.
    if name == 'ErrorOccurred':
        return 0
    if name == 'Notification':
        return 2 if event.get('notification_type') in ('permission_prompt', 'elicitation_dialog') else None
    if name == 'PreToolUse':
        tool = event.get('toolName', event.get('tool_name'))
        return 2 if tool in ('ask_user', 'AskUserQuestion') else 1
    return state(dict(event, hook_event_name=name))

def main():
    sequence = time.monotonic_ns()
    try:
        tag = {'codex': 1, 'claude': 2, 'copilot': 6}[sys.argv[1]]
        raw = sys.stdin.buffer.read(LIMIT + 1)
        if len(raw) > LIMIT:
            return
        event = json.loads(raw)
        status = copilot_state(event, sys.argv[2]) if sys.argv[1] == 'copilot' else state(event)
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
