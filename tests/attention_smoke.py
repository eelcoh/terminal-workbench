#!/usr/bin/env python3
"""Real runtime with native fixtures and hook payloads, not live CLI validation."""
from pathlib import Path
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import uuid
from agent_smoke import snapshot
from session_smoke import attach, send, read_frame, wait_until

ROOT = Path(__file__).resolve().parent.parent

def main():
    runtime = str(Path(sys.argv[1]).resolve())
    for agent, identity_tag in [('codex',1),('copilot',6)]:
        with tempfile.TemporaryDirectory(prefix='wb-attention-') as directory:
            root = Path(directory)
            path = root / 'session.sock'
            fixture = root / agent
            shutil.copyfile('/usr/bin/sleep', fixture)
            fixture.chmod(0o700)
            process = subprocess.Popen([runtime, str(path), '/bin/sh'], cwd=root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            viewer = legacy = watch = None
            try:
                wait_until(lambda: path.with_suffix('.ctl').exists())
                viewer, _, _ = attach(path)
                send(viewer, 16, f"sh -c 'echo $$ > agent.pid; exec {fixture} 120'\n".encode())
                wait_until(lambda: snapshot(runtime, path)['agent'] == agent)
                group = os.getpgid(int((root / 'agent.pid').read_text()))
                token = uuid.uuid4().hex
                environment = dict(os.environ, WORKBENCH_SESSION_SOCKET=str(path), WORKBENCH_AGENT_GROUP=str(group), WORKBENCH_AGENT_RUN=token)
                for version in ('legacy', 'watch'):
                    connection = socket.socket(socket.AF_UNIX)
                    connection.settimeout(5)
                    connection.connect(str(path.with_suffix('.ctl')))
                    connection.sendall(b'WATCH' if version == 'legacy' else b'WAT2\n')
                    assert read_frame(connection) == ((5, bytes([identity_tag])) if version == 'legacy' else (6, bytes([identity_tag,0])))
                    if version == 'legacy': legacy = connection
                    else: watch = connection
                def hook(event, **fields):
                    extra=[]
                    if agent=='copilot':
                        names={'UserPromptSubmit':'userPromptSubmitted','PostToolUse':'postToolUse','Stop':'agentStop','Notification':'notification'}
                        if event=='PermissionRequest':event='Notification';fields['notification_type']='permission_prompt'
                        extra=[names.get(event,'notification')]
                    result = subprocess.run(['python3', str(ROOT / 'scripts/agent-hook.py'), agent, *extra], input=json.dumps(dict(hook_event_name=event, **fields)), text=True, capture_output=True, env=environment, timeout=3)
                    assert result.returncode == 0 and result.stdout == result.stderr == ''
                assert snapshot(runtime, path)['status'] == 'unknown'
                for event, status, state_tag in [('UserPromptSubmit', 'working', 1), ('PermissionRequest', 'waiting', 2), ('PostToolUse', 'working', 1), ('Stop', 'ready', 3)]:
                    hook(event)
                    assert read_frame(watch) == (6, bytes([identity_tag, state_tag]))
                    assert snapshot(runtime, path)['status'] == status
                hook('UserPromptSubmit', agent_id='child')
                hook('invented')
                assert snapshot(runtime, path)['status'] == 'ready'
                # Bad group, run token and stale sequence cannot overwrite state.
                for candidate_group, candidate_token, sequence in [(group + 1, token, 2**63), (group, uuid.uuid4().hex, 2**63), (group, token, 1)]:
                    with socket.socket(socket.AF_UNIX) as control:
                        control.settimeout(2)
                        control.connect(str(path.with_suffix('.ctl')))
                        control.sendall(b'STATE' + struct.pack('!BBi16sQ', identity_tag, 2, candidate_group, bytes.fromhex(candidate_token), sequence))
                        assert control.recv(16) == b'ERROR\n'
                viewer.close(); viewer = None
                hook('PermissionRequest')
                assert read_frame(watch) == (6, bytes([identity_tag,2]))
                viewer, _, _ = attach(path)
                assert snapshot(runtime, path)['status'] == 'waiting'
                legacy.settimeout(0.1)
                try:
                    read_frame(legacy)
                    raise AssertionError('status leaked into legacy identity subscription')
                except socket.timeout:
                    pass
                legacy.settimeout(5)
                send(viewer, 16, b'\x03')
                assert read_frame(legacy) == (5, b'\x00')
                assert read_frame(watch) == (6, b'\x00\x00')
                hook('Stop')
                assert snapshot(runtime, path)['status'] == 'unknown'
                for raw in ('[]', '{', 'x' * (1024 * 1024 + 1)):
                    result = subprocess.run(['python3', str(ROOT / 'scripts/agent-hook.py'), agent, *(['notification'] if agent == 'copilot' else [])], input=raw, text=True, capture_output=True, env=environment, timeout=3)
                    assert result.returncode == 0 and result.stdout == result.stderr == ''
            finally:
                for connection in (viewer, legacy, watch):
                    if connection: connection.close()
                subprocess.run([runtime, 'stop', str(path)], timeout=12, capture_output=True)
                process.wait(timeout=5)
    # Execute each launcher against an argument-capture fixture, preserving argv.
    with tempfile.TemporaryDirectory(prefix='wb-launch-') as directory:
        root = Path(directory)
        for agent in ('codex', 'claude'):
            fake = root / agent
            fake.write_text('#!/usr/bin/env python3\nimport json,os,sys\nprint(json.dumps({"args":sys.argv[1:],"group":os.environ["WORKBENCH_AGENT_GROUP"],"run":os.environ["WORKBENCH_AGENT_RUN"]}))\n')
            fake.chmod(0o700)
            environment = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'], WORKBENCH_SESSION_SOCKET='/tmp/fixture.sock')
            result = subprocess.run(['python3', str(ROOT / 'scripts/workbench-agent.py'), agent, '--resume', 'a spaced argument'], env=environment, capture_output=True, text=True, check=True)
            capture = json.loads(result.stdout)
            assert capture['args'][-2:] == ['--resume', 'a spaced argument']
            assert int(capture['group']) == os.getpgrp() and len(bytes.fromhex(capture['run'])) == 16
            if agent == 'claude':
                assert 'PermissionRequest' in json.loads(capture['args'][1])['hooks']
            else:
                import tomllib
                args = capture['args']
                assert args[0] == '--no-daemon'
                for index in range(1, len(args) - 2, 2):
                    assert args[index] == '-c'
                    assert tomllib.loads(args[index + 1])['hooks']
    print('PASS: hook lifecycle, stale/child rejection, unknown fallback, detach/reattach, versioned metadata, clearing, silent failures and invocation-local launchers')

if __name__ == '__main__': main()
