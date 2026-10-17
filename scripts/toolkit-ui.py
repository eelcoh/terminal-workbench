"""Launch the isolated UI study with six disposable shells, never saved sessions."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

runtime,viewer=[str(Path(value).resolve()) for value in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='wb-ui-',dir='/tmp') as directory:
    root=Path(directory);servers=[];sockets=[]
    try:
        with (root/'runtime.log').open('w') as log:
            for i in range(6):
                sock=root/f'{i}.sock';sockets.append(sock)
                server=subprocess.Popen([runtime,str(sock)],cwd=Path.home(),stdout=log,stderr=log);servers.append(server)
                deadline=time.monotonic()+15
                while not sock.with_suffix('.ctl').exists():
                    if server.poll() is not None or time.monotonic()>deadline:raise RuntimeError('Disposable shell did not start')
                    time.sleep(.02)
            config=root/'sessions.json';config.write_text(json.dumps([str(sock.with_suffix('.ctl')) for sock in sockets]))
            print('UI study: six disposable shells. Closing stops them. No saved Workbench sessions are opened.',flush=True)
            subprocess.run([viewer,str(config)],check=True)
    finally:
        for sock,server in zip(sockets,servers):
            subprocess.run([runtime,'stop',str(sock)],capture_output=True,timeout=12)
            try:server.wait(timeout=5)
            except subprocess.TimeoutExpired:server.kill();server.wait()
