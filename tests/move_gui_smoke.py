#!/usr/bin/env python3
"""Real GUI moves retain live sessions and persist placement."""
from pathlib import Path
import fcntl
import json
import os
import select
import subprocess
import sys
import tempfile
import termios
import time
from session_smoke import attach, send, wait_until
ROOT = Path(__file__).resolve().parent.parent

def main():
    runtime, binary = [str(Path(a).resolve()) for a in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-drag-') as directory:
        root = Path(directory); state = root / 'sessions'; state.mkdir()
        data = {'version': 1, 'workspaces': [
            {'id': 'a', 'name': 'Source', 'terminals': [{'id': 'one', 'name': 'One'}]},
            {'id': 'b', 'name': 'Empty', 'terminals': []},
            {'id': 'c', 'name': 'Other', 'terminals': [{'id': 'two', 'name': 'Two'}, {'id': 'three', 'name': 'Three'}]},
            {'id': 'd', 'name': 'Full', 'terminals': [{'id': f'f{i}', 'name': f'Full {i}'} for i in range(8)]}]}
        (state / 'workspaces.json').write_text(json.dumps(data))
        env = dict(os.environ, SHELL='/bin/sh', LIBGL_ALWAYS_SOFTWARE='1', WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),
            WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'), WORKBENCH_STATE_DIR=str(state), WORKBENCH_RUNTIME=runtime, WORKBENCH_PROJECT=str(root))
        env.pop('WAYLAND_DISPLAY', None); env.pop('WORKBENCH_SOCKET', None)
        processes = []
        with (root/'test.log').open('w+') as log:
            try:
                subprocess.run(['python3', env['WORKBENCH_CATALOG_SCRIPT'], '--state-dir', str(state), '--runtime', runtime, '--cwd', str(root)], env=env, stdout=log, stderr=log, check=True, timeout=20)
                pids, sockets = {}, {}
                for w in data['workspaces']:
                    for terminal in w['terminals']:
                        identity = terminal['id']; connection, _, _ = attach(state/f'{identity}.sock')
                        send(connection, 16, f'keep={identity}; echo $$ > {identity}.pid\n'.encode())
                        wait_until(lambda: (root/f'{identity}.pid').exists())
                        pids[identity] = int((root/f'{identity}.pid').read_text())
                        sockets[identity] = (state/f'{identity}.sock').stat().st_ino
                        connection.close()
                read_fd, write_fd = os.pipe()
                xvfb = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1440x1000x24', '-nolisten', 'tcp'], pass_fds=(write_fd,), stdout=log, stderr=log)
                processes.append(xvfb); os.close(write_fd)
                assert select.select([read_fd], [], [], 5)[0]
                env['DISPLAY'] = ':' + os.read(read_fd,32).decode().strip(); os.close(read_fd)
                def start():
                    viewer = subprocess.Popen([binary], env=env, cwd=root, stdout=log, stderr=log); processes.append(viewer)
                    def find():
                        assert viewer.poll() is None
                        r = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(viewer.pid)], env=env, capture_output=True,text=True)
                        return r.stdout.strip().splitlines()[-1] if r.returncode == 0 else None
                    window = wait_until(find,timeout=10); cmd('windowfocus',window); time.sleep(.5)
                    return viewer, window
                def cmd(*args):
                    r = subprocess.run(['xdotool', *map(str,args)],env=env,capture_output=True,text=True,timeout=5)
                    r.check_returncode(); return r.stdout.strip()
                viewer, window = start()
                def key(k):
                    cmd('keydown','--window',window,k); time.sleep(.06); cmd('keyup','--window',window,k); time.sleep(.1)
                def click(x,y):
                    cmd('mousemove','--window',window,x,y); cmd('mousedown',1); time.sleep(.06); cmd('mouseup',1); time.sleep(.1)
                def drag(x,y,tx,ty, release=True):
                    cmd('mousemove','--window',window,x,y); cmd('mousedown',1); time.sleep(.08)
                    cmd('mousemove','--window',window,tx,ty); time.sleep(.12)
                    if release: cmd('mouseup',1); time.sleep(.25)
                def metadata(): return json.loads((state/'workspaces.json').read_text())
                def ids(w): return [t['id'] for t in metadata()['workspaces'][w]['terminals']]
                def geometry():
                    result = {}
                    for identity,pid in pids.items():
                        fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                        try: result[identity]=fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                        finally: os.close(fd)
                    return result
                def alive():
                    for identity,pid in pids.items():
                        os.kill(pid,0); assert (state/f'{identity}.sock').stat().st_ino == sockets[identity]
                def verify(identity, name):
                    cmd('type','--clearmodifiers','--window',window,'--delay',10,'--',f'printf "%s:%s" "$$" "$keep" > {name}'); key('Return')
                    actual=wait_until(lambda: (root/name).exists() and (root/name).read_text())
                    assert actual == f'{pids[identity]}:{identity}', (identity,actual)
                key('ctrl+shift+o'); time.sleep(.25); before=geometry()
                # Empty destination and empty source retain their workspace rows.
                drag(170,300,170,560); wait_until(lambda: ids(1)==['one'])
                assert ids(0)==[] and geometry()==before; alive()
                key('F12'); time.sleep(.15)
                artifacts=ROOT/'.local/test-results'; artifacts.mkdir(exist_ok=True)
                import shutil
                shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1],artifacts/'overview-moved.png')
                # Escape and releases outside the stage leave saved metadata alone.
                saved=(state/'workspaces.json').read_bytes()
                drag(170,560,470,310,False); key('Escape'); cmd('mouseup',1); time.sleep(.15)
                assert (state/'workspaces.json').read_bytes()==saved
                drag(170,560,10,310); assert (state/'workspaces.json').read_bytes()==saved
                # Wheel while holding reveals a populated destination; insert before Two.
                drag(170,560,170,560,False)
                cmd('mousemove','--window',window,350,560); time.sleep(.08); cmd('click',5); time.sleep(.12)
                cmd('mousemove','--window',window,80,560); time.sleep(.1); cmd('mouseup',1)
                wait_until(lambda: ids(2)==['one','two','three']); assert ids(1)==[]
                assert geometry()==before; alive()
                # Same-workspace reorder: first card goes after the third.
                drag(170,560,890,560); wait_until(lambda: ids(2)==['two','three','one'])
                assert geometry()==before
                # Holding near the top edge reveals an offscreen empty destination.
                drag(760,560,600,166,False); time.sleep(.8)
                cmd('mousemove','--window',window,600,560); time.sleep(.1); cmd('mouseup',1)
                wait_until(lambda: ids(1)==['one']); assert ids(2)==['two','three']
                # A filtered-out populated row appends at its actual catalog count.
                cmd('type','--window',window,'--delay',30,'--','One'); time.sleep(.15)
                drag(170,300,500,560,False); cmd('click',5); time.sleep(.15)
                cmd('mouseup',1); wait_until(lambda: ids(2)==['two','three','one'])
                assert ids(1)==[] and geometry()==before; alive()
                key('ctrl+a'); key('BackSpace'); time.sleep(.1)
                # Filtered positions use catalog indices, and all rows become targets.
                cmd('type','--window',window,'--delay',30,'--','One'); time.sleep(.15)
                drag(170,300,500,560,False); time.sleep(.1)
                cmd('click',5); time.sleep(.1) # Source + Empty -> Empty + Other
                cmd('click',5); time.sleep(.1) # Other + Full
                cmd('mousemove','--window',window,500,560); time.sleep(.1); cmd('mouseup',1); time.sleep(.2)
                assert ids(2)==['two','three','one'] and len(ids(3))==8
                key('ctrl+a'); key('BackSpace'); time.sleep(.1)
                # Select moved terminal in its new row, enter it, and check shell state.
                key('Escape'); time.sleep(.25); key('ctrl+shift+o'); time.sleep(.25)
                cmd('type','--window',window,'--delay',30,'--','Other'); time.sleep(.15)
                click(760,300); time.sleep(.25); verify('one','after-move')
                click(1196,16); assert viewer.wait(timeout=5)==0; alive()
                # Reopen from saved placement, with the same running shells.
                viewer,window=start(); key('ctrl+shift+o'); time.sleep(.25)
                cmd('type','--window',window,'--delay',30,'--','Other'); time.sleep(.15)
                click(760,300); time.sleep(.25); verify('one','after-reopen')
                click(1196,16); assert viewer.wait(timeout=5)==0; alive()
                print('PASS: empty-workspace drop, populated insert, reorder, escape/outside/full cancellation, edge scrolling, filtered insertion, persistence and unchanged live shell PID/state/socket/geometry')
            finally:
                for process in reversed(processes):
                    if process.poll() is None: process.terminate(); process.wait(timeout=5)
                for path in state.glob('*.sock'): subprocess.run([runtime,'stop',str(path)],capture_output=True,timeout=12)
                if sys.exc_info()[0]: log.seek(0); print(log.read()[-5000:],file=sys.stderr)
if __name__=='__main__': main()
