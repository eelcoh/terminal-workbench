#!/usr/bin/env python3
"""Watch existing runtime metadata; never attach terminals or start services."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path
import argparse
import json
import os
import selectors
import socket
import stat
import struct
import time
from catalog_files import check_identities, socket_path

AGENTS = {0: None, 1: 'codex', 2: 'claude', 3: 'gemini', 4: 'opencode', 5: 'aider', 6: 'copilot', 7: 'pi', 255: None}
STATES = {0: 'unknown', 1: 'working', 2: 'waiting', 3: 'ready'}
CATALOG_LIMIT = 256 * 1024

def entries(directory):
    with (directory / 'workspaces.json').open('rb') as file:
        raw = file.read(CATALOG_LIMIT + 1)
    if len(raw) > CATALOG_LIMIT:
        raise ValueError('Catalog is too large')
    data = json.loads(raw)
    check_identities(data)
    return [(dict(workspace_id=w['id'], workspace=w['name'], terminal_id=t['id'], terminal=t['name']),
             socket_path(directory, t).with_suffix('.ctl'))
            for w in data['workspaces'] for t in w['terminals']]

def subscribe(entry):
    identity, path = entry
    connection = None
    try:
        info = path.lstat()
        if not stat.S_ISSOCK(info.st_mode) or info.st_uid != os.getuid():
            raise ValueError('Not a user-owned control socket')
        # Capability probing is read-only and bounded; legacy runtimes retain WAT2.
        version = b'WAT2\n'
        with socket.socket(socket.AF_UNIX) as probe:
            probe.settimeout(.4)
            try:
                probe.connect(str(path));probe.sendall(b'CAPS\n');raw = bytearray()
                while len(raw) <= 4096 and not raw.endswith(b'\n'):
                    part = probe.recv(min(1024,4097-len(raw)))
                    if not part:break
                    raw.extend(part)
                if len(raw) <= 4096:
                    capabilities = json.loads(raw).get('capabilities',[])
                    if isinstance(capabilities,list) and 'agent-watch-v3' in capabilities:
                        version = b'WAT3\n'
            except (OSError,ValueError,AttributeError,TypeError):
                pass
        connection = socket.socket(socket.AF_UNIX)
        connection.settimeout(.4)
        connection.connect(str(path))
        connection.sendall(version)
        connection.setblocking(False)
        return identity, connection, None
    except (OSError, ValueError) as error:
        if connection is not None:
            connection.close()
        return identity, None, str(error)

def emit(identity, **fields):
    print(json.dumps(dict(at=datetime.now(timezone.utc).isoformat(), **identity, **fields)), flush=True)

def watch(directory, seconds):
    initial = entries(directory)  # One catalog snapshot; no locks, seed or management calls.
    with selectors.DefaultSelector() as selector:
        with ThreadPoolExecutor(max_workers=4) as executor:
            for identity, connection, error in executor.map(subscribe, initial):
                if connection is None:
                    emit(identity, event='unavailable', reason=error)
                else:
                    selector.register(connection, selectors.EVENT_READ,
                                      dict(identity=identity, buffer=bytearray(), last=None, received=False))
        started = time.monotonic()
        deadline = started + seconds
        def drop(key, reason):
            emit(key.data['identity'], event='unavailable', reason=reason)
            selector.unregister(key.fileobj)
            key.fileobj.close()
        try:
            while selector.get_map() and time.monotonic() < deadline:
                for key, _ in selector.select(min(.25, max(0, deadline - time.monotonic()))):
                    state = key.data
                    try:
                        data = key.fileobj.recv(4096)
                        if not data:
                            drop(key, 'Metadata connection closed')
                            continue
                        state['buffer'].extend(data)
                        while len(state['buffer']) >= 5:
                            kind, length = struct.unpack('!BI', state['buffer'][:5])
                            if (kind, length) != (6, 2):
                                raise ValueError('Unsupported metadata frame; runtime may predate lifecycle support')
                            if len(state['buffer']) < 7:
                                break
                            agent, status = state['buffer'][5:7]
                            del state['buffer'][:7]
                            if agent not in AGENTS or status not in STATES:
                                raise ValueError('Unsupported agent/status tag')
                            state['received'] = True
                            tags = (agent, status)
                            if tags != state['last']:
                                emit(state['identity'], event='state', agent=AGENTS[agent],
                                     identity_available=agent != 255, status=STATES[status])
                                state['last'] = tags
                    except BlockingIOError:
                        pass
                    except (OSError, ValueError) as error:
                        drop(key, str(error))
                for key in list(selector.get_map().values()):
                    if not key.data['received'] and time.monotonic() > started + 2:
                        drop(key, 'No metadata reply; runtime may predate lifecycle support')
        finally:
            for key in list(selector.get_map().values()):
                if not key.data['received']:
                    emit(key.data['identity'], event='unavailable', reason='No metadata received during observation')
                selector.unregister(key.fileobj)
                key.fileobj.close()

def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state-dir', type=Path, default=Path(os.environ.get('WORKBENCH_STATE_DIR', root / '.local/sessions')))
    parser.add_argument('--seconds', type=float, default=120)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 3600:
        parser.error('--seconds must be between 1 and 3600')
    try:
        watch(args.state_dir, args.seconds)
    except KeyboardInterrupt:
        pass
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')

if __name__ == '__main__':
    main()
