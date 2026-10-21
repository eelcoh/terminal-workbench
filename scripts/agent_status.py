#!/usr/bin/env python3
"""Read agent metadata without starting services, attaching PTYs or sending input."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import json
import os
import socket
import stat
import struct
import subprocess
from datetime import datetime
from workspace_catalog import validate, socket_path

LIMIT = 4096

def read_reply(path, command, framed=False):
    info = path.stat()
    if not stat.S_ISSOCK(info.st_mode) or info.st_uid != os.getuid():
        raise ValueError('Not a user-owned control socket')
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(1)
        connection.connect(str(path))
        connection.sendall(command)
        data = bytearray()
        while len(data) <= LIMIT:
            chunk = connection.recv(min(1024, LIMIT + 1 - len(data)))
            if not chunk: break
            data.extend(chunk)
            if framed and len(data) >= 5:
                kind, length = data[0], struct.unpack('!I', data[1:5])[0]
                if (kind, length) != (6, 2):
                    raise ValueError('No lifecycle subscription; runtime may predate status support')
                if len(data) >= 7: return {'agent_tag':data[5], 'status_tag':data[6]}
            elif not framed and b'\n' in data:
                return json.loads(data.split(b'\n',1)[0])
        raise ValueError('Missing or oversized reply; this runtime may not support the requested metadata')

def inspect(entry):
    workspace, terminal, directory = entry
    path = socket_path(directory, terminal).with_suffix('.ctl')
    result = dict(workspace=workspace['name'], terminal=terminal['name'], id=terminal['id'])
    for field, command, framed in [('runtime',b'CAPS\n',False),('identity',b'INFO\n',False),('viewer_metadata',b'WAT2\n',True)]:
        try:
            capabilities = result.get('runtime',{}).get('capabilities',[])
            if field == 'viewer_metadata' and isinstance(capabilities,list) and 'agent-watch-v3' in capabilities:
                command = b'WAT3\n'
            result[field] = read_reply(path, command, framed)
        except (OSError, ValueError) as error:
            result[field] = {'unavailable':str(error)}
    pid = result.get('runtime',{}).get('runtime_pid')
    if isinstance(pid,int) and pid > 1:
        try:
            start = subprocess.run(['/bin/ps','-p',str(pid),'-o','lstart='],capture_output=True,text=True,timeout=2)
            if start.returncode == 0: result['runtime_started'] = start.stdout.strip()
        except (OSError,subprocess.TimeoutExpired): pass
    return result

def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state-dir',type=Path,default=root/'.local/sessions')
    args = parser.parse_args()
    # Read the existing catalog directly: no seed, lock file, startup or recovery.
    data = json.loads((args.state_dir/'workspaces.json').read_text())
    validate(data)
    runtime = root/'target/debug/workbench-session'
    if runtime.exists():
        print('Built runtime modified: '+datetime.fromtimestamp(runtime.stat().st_mtime).astimezone().isoformat())
    entries = [(w,t,args.state_dir) for w in data['workspaces'] for t in w['terminals']]
    with ThreadPoolExecutor(max_workers=4) as executor:
        for result in executor.map(inspect,entries): print(json.dumps(result))

if __name__ == '__main__':
    main()
