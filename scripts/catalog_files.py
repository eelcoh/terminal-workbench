"""Read workspaces.json for development tools, and seed it in tests.

The Rust session service owns validation and every catalog transaction. Tools
read the committed file; tests write one only before the service uses it.
"""
import json
import os
import re
import tempfile
from pathlib import Path


def load(directory):
    return json.loads((Path(directory) / 'workspaces.json').read_text())


IDENTITY = re.compile(r'[A-Za-z0-9_-]{1,40}')


def check_identities(data):
    """Reject a catalog whose identities could escape the session directory."""
    if not isinstance(data, dict) or data.get('version') != 1 or not isinstance(data.get('workspaces'), list):
        raise ValueError('Unsupported workspace metadata')
    for workspace in data['workspaces']:
        for entity in [workspace, *workspace.get('terminals', [])]:
            if not isinstance(entity.get('id'), str) or not IDENTITY.fullmatch(entity['id']):
                raise ValueError('Invalid session identity')
    return data


def terminals(data):
    return [t for w in data['workspaces'] for t in w['terminals']]


def socket_path(directory, terminal):
    return Path(directory) / (terminal['id'] + '.sock')


def atomic_write(path, text):
    path = Path(path)
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name + '.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as file:
            file.write(text)
            file.flush()
            os.fsync(file.fileno())
        os.replace(temporary, path)
    except BaseException:
        os.unlink(temporary)
        raise


def save(directory, data):
    atomic_write(Path(directory) / 'workspaces.json', json.dumps(data, indent=2) + '\n')
