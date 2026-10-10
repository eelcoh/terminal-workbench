#!/usr/bin/env python3
"""Viewer failures must produce a current log without disturbing session files."""
from pathlib import Path
import os
import select
import socket
import subprocess
import sys
import tempfile
import time
import signal
from session_smoke import wait_until

ROOT = Path(__file__).resolve().parent.parent
LOADER = "import importlib.util,sys; from pathlib import Path; sys.path.insert(0, str(Path(sys.argv[1]).parent)); spec=importlib.util.spec_from_file_location('launcher',sys.argv[1]); module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module); sys.exit(module.launch_viewer(Path(sys.argv[2]),Path(sys.argv[3]),Path(sys.argv[3]),dict(__import__('os').environ),detach=module.inside_workbench(__import__('os').environ)))"


def launch(viewer, state, env, *, nested=False):
    env=dict(env)
    if not nested:
        env.pop('WORKBENCH_SESSION_SOCKET',None)
        env.pop('TERM_PROGRAM',None)
    return subprocess.run([sys.executable, '-c', LOADER, str(ROOT/'scripts/run-spike.py'), str(viewer), str(state)],
                          env=env, capture_output=True, timeout=20)


def main():
    with tempfile.TemporaryDirectory(prefix='wb-launch-') as directory:
        state=Path(directory)
        shell_log=state/'shell.log'; shell_log.write_bytes(b'existing session output')
        viewer=state/'fixture'
        viewer.write_text(f'#!{sys.executable}\nimport os,sys\nprint("viewer stdout",flush=True)\nprint("viewer stderr",file=sys.stderr,flush=True)\nsys.exit(int(os.environ.get("VIEWER_EXIT", "1")))\n')
        viewer.chmod(0o700)
        result=launch(viewer,state,os.environ)
        log=state/'viewer.log'; first=log.read_bytes()
        assert result.returncode==1 and b'viewer stdout' in result.stdout and b'viewer stderr' in result.stdout
        assert b'Viewer failed (exit 1)' in result.stderr and str(log).encode() in result.stderr
        assert b'viewer stdout' in first and b'viewer stderr' in first and b'Viewer exit code: 1' in first
        result=launch(viewer,state,dict(os.environ,VIEWER_EXIT='0'))
        assert result.returncode==0 and b'Viewer failed' not in result.stderr
        assert log.read_bytes().startswith(first) and b'Viewer exit code: 0' in log.read_bytes()
        result=launch(state/'missing',state,os.environ)
        assert result.returncode==1 and b'Could not launch viewer' in log.read_bytes()
        assert shell_log.read_bytes()==b'existing session output'
        # Reproduce launching a second GUI from inside a retained terminal:
        # a live child must not hold the launcher or retain its stdin/stdio pipe.
        nested_viewer=state/'nested-viewer'
        nested_viewer.write_text(f'#!{sys.executable}\n'+"import os,time\nfrom pathlib import Path\n"
            +"assert os.read(0,1)==b''\nassert os.getsid(0)==os.getpid()\n"
            +"Path('nested.pid').write_text(str(os.getpid()))\n"
            +"print('independent viewer output',flush=True)\n"
            +"while not Path('finish-nested').exists():time.sleep(.01)\n")
        nested_viewer.chmod(0o700)
        started=time.monotonic()
        result=launch(nested_viewer,state,dict(os.environ,WORKBENCH_SESSION_SOCKET=str(state/'retained.sock')),nested=True)
        assert result.returncode==0 and time.monotonic()-started<3
        pid=int(wait_until(lambda:(state/'nested.pid').exists() and (state/'nested.pid').read_text()))
        try:
            os.kill(pid,0)
            assert b'Viewer started independently' in result.stdout
            assert b'independent viewer output' not in result.stdout
            wait_until(lambda:b'independent viewer output' in log.read_bytes())
            assert shell_log.read_bytes()==b'existing session output'
        finally:
            (state/'finish-nested').touch()
            time.sleep(.1)
            try:os.kill(pid,signal.SIGTERM)
            except ProcessLookupError:pass
        # Startup failures still report their exit status in detached mode.
        result=launch(viewer,state,dict(os.environ,TERM_PROGRAM='terminal-workbench',VIEWER_EXIT='1'),nested=True)
        assert result.returncode==1 and b'Viewer failed' in result.stderr
        if len(sys.argv)>1:
            # A leftover Unix socket reproduces an immediate attachment failure.
            sock=state/'stale.sock'
            with socket.socket(socket.AF_UNIX) as server: server.bind(str(sock))
            inode=sock.stat().st_ino
            catalog=state/'workspaces.tsv'
            catalog.write_text(f'workbench-catalog-v3\nW\tw0\tMain\nT\tw0\tstale\tShell\t{sock}\n')
            read_fd,write_fd=os.pipe()
            with (state/'xvfb.log').open('wb') as output:
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1280x900x24','-nolisten','tcp'],pass_fds=(write_fd,),stdout=output,stderr=output)
                os.close(write_fd)
                try:
                    assert select.select([read_fd],[],[],5)[0]
                    display=':'+os.read(read_fd,32).decode().strip()
                    env=dict(os.environ,DISPLAY=display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(catalog))
                    for key in ('WAYLAND_DISPLAY','WORKBENCH_THEME','WORKBENCH_STATE_DIR','WORKBENCH_CATALOG_SCRIPT'): env.pop(key,None)
                    result=launch(Path(sys.argv[1]).resolve(),state,env)
                    assert result.returncode==1, result.stdout.decode(errors='replace')
                    assert b'Could not attach to session socket' in result.stdout and str(sock).encode() in result.stdout, result.stdout.decode(errors='replace')
                    assert b'Could not initialize terminal stale' in result.stdout
                    assert sock.stat().st_ino==inode and shell_log.read_bytes()==b'existing session output'
                    assert result.stdout.split(b'Viewer log:')[1].split(b'\n',1)[1] in log.read_bytes()
                finally:
                    os.close(read_fd); xvfb.terminate(); xvfb.wait(timeout=5)
    print('PASS: viewer stdout/stderr, append on relaunch, failure/normal exit codes, missing executable, session preservation, independent nested launch and detached startup failures' + (', real viewer stale-socket diagnostics' if len(sys.argv)>1 else ''))


if __name__=='__main__': main()
