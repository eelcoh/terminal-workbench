#!/usr/bin/env python3
"""Live theme preview, save/cancel, parser errors, session continuity and UI samples."""
from gui_smoke import close_window
from chrome_gui_actions import action as chrome_action, workspace as chrome_workspace
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
import struct
import zlib
from session_smoke import attach, send, wait_until
ROOT = Path(__file__).resolve().parent.parent

from functools import lru_cache

@lru_cache(maxsize=32)
def _png_row(path, py):
    # Decode Raylib's PNG screenshots with stdlib only; no imaging dependency.
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset, compressed = 8, bytearray()
    while offset < len(data):
        length = struct.unpack('!I', data[offset:offset + 4])[0]
        kind, body = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + length]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('!IIBBBBB', body)
            assert depth == 8 and color in (2, 6) and interlace == 0
        if kind == b'IDAT': compressed.extend(body)
        offset += length + 12
    raw = zlib.decompress(compressed)
    channels, offset = (4 if color == 6 else 3), 0
    prior = bytearray(width * channels)
    for y in range(height):
        filter_kind = raw[offset]; offset += 1
        row = bytearray(raw[offset:offset + len(prior)]); offset += len(prior)
        for index in range(len(row)):
            left = row[index - channels] if index >= channels else 0
            above = prior[index]
            upper_left = prior[index - channels] if index >= channels else 0
            if filter_kind == 1: predictor = left
            elif filter_kind == 2: predictor = above
            elif filter_kind == 3: predictor = (left + above) // 2
            elif filter_kind == 4:
                value = left + above - upper_left
                distances = [abs(value - v) for v in (left, above, upper_left)]
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                assert filter_kind == 0
                predictor = 0
            row[index] = (row[index] + predictor) & 255
        if y == py: return bytes(row),channels
        prior=row
    raise AssertionError('Pixel outside screenshot')

def pixel(path, px, py):
    row,channels=_png_row(path,py)
    return tuple(row[px*channels:px*channels+3])

def main():
    runtime, binary = [str(Path(a).resolve()) for a in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-theme-') as directory:
        root = Path(directory); state = root / 'sessions'; state.mkdir()
        pane_count=int(sys.argv[3]) if len(sys.argv)>3 else 15
        data={'version':1,'workspaces':[{'id':f'w{w}','name':['Workbench','Exploration','Review'][w],
            'terminals':[{'id':f'w{w}p{p}','name':['Build','Tests','Agent','Notes','Shell'][p]} for p in range(min(5,pane_count-w*5))]} for w in range((pane_count+4)//5)]}
        (state / 'workspaces.json').write_text(json.dumps(data))
        env = dict(os.environ, SHELL='/bin/sh', LIBGL_ALWAYS_SOFTWARE='1', WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),
            WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'), WORKBENCH_STATE_DIR=str(state), WORKBENCH_RUNTIME=runtime, WORKBENCH_PROJECT=str(root))
        env.pop('WAYLAND_DISPLAY', None); env.pop('WORKBENCH_SOCKET', None); env.pop('WORKBENCH_THEME', None)
        processes = []
        viewer = None
        with (root/'test.log').open('w+') as log:
            try:
                subprocess.run(['python3', env['WORKBENCH_CATALOG_SCRIPT'], '--state-dir', str(state), '--runtime', runtime, '--cwd', str(root), '--command', 'session.recover'], env=env, stdout=log, stderr=log, check=True, timeout=20)
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
                xvfb = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1440x1000x24', '-noreset', '-nolisten', 'tcp'], pass_fds=(write_fd,), stdout=log, stderr=log)
                processes.append(xvfb); os.close(write_fd)
                assert select.select([read_fd], [], [], 5)[0]
                with os.fdopen(read_fd) as display_pipe: env['DISPLAY'] = ':' + display_pipe.readline().strip()
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
                def capture(name):
                    key('F12'); time.sleep(.12)
                    newest=max(root.glob('screenshot*.png'),key=lambda p:p.stat().st_mtime_ns)
                    artifacts=ROOT/'.local/test-results'; artifacts.mkdir(exist_ok=True)
                    import shutil
                    target=artifacts/f'ux-{pane_count}-{name}.png'; shutil.copyfile(newest,target)
                    return target
                def preferences():
                    sections={}; current=None
                    for line in (state/'appearance.state').read_text().splitlines():
                        if line.startswith('@'): current=line[1:]; sections[current]={}
                        elif current and '=' in line:
                            k,v=line.split('=',1); sections[current][k.strip()]=v.strip()
                    return sections
                def choose(scope,name,apply=True):
                    if scope==2:
                        click(629,62);click(725,135)
                    else:chrome_action(cmd,window,'Change appearance')
                    cmd('type','--clearmodifiers','--window',window,'--delay',20,'--',name)
                    if apply:key('Return')
                def cpu_sample():
                    begin=time.monotonic()
                    def ticks(): return sum(map(int,Path(f'/proc/{viewer.pid}/stat').read_text().split(') ',1)[1].split()[11:13]))
                    baseline=ticks(); time.sleep(2)
                    return round(100*(ticks()-baseline)/os.sysconf('SC_CLK_TCK')/(time.monotonic()-begin),1)
                before=geometry()
                chrome_action(cmd,window,'Change appearance');opening=capture('workspace-picker-night')
                surface=pixel(opening,210,180)
                cmd('type','--clearmodifiers','--window',window,'--delay',20,'--','Dawn')
                day=capture('workspace-picker-preview')
                assert pixel(day,210,180)==surface,'picker changed its own surface during preview'
                assert pixel(day,100,730)[0]>140 and pixel(day,10,300)[0]>140,'workspace/chrome did not preview together'
                assert geometry()==before;alive()
                key('ctrl+a');cmd('type','--clearmodifiers','--window',window,'--delay',10,'--','no-matching-palette')
                key('Return');assert not (state/'appearance.state').exists()
                key('Escape');verify('w0p0','cancelled-search')
                assert pixel(capture('workspace-cancelled'),100,730)[0]<60
                key('ctrl+u')
                choose(1,'Dawn',apply=False);(state/'appearance.state').mkdir();key('Return')
                assert (state/'appearance.state').is_dir();capture('save-error');(state/'appearance.state').rmdir();key('Escape')
                key('ctrl+shift+o');time.sleep(.25);before=geometry()
                click(292,74);key('Right');key('Return')
                wait_until(lambda:(state/'appearance.state').exists())
                assert preferences()['workspace\tw0']['background']=='#f4f1eb'
                assert preferences()['application']['background']=='#15191f' and preferences()['default']['background']=='#15191f'
                assert geometry()==before;alive();capture('overview-dawn');day_cpu=cpu_sample()
                click(292,74);key('Left');key('Return');capture('overview-night');night_cpu=cpu_sample()
                key('Escape');time.sleep(.25);verify('w0p0','after-theme')
                close_window(window,env);assert viewer.wait(timeout=5)==0;alive()
                viewer,window=start();verify('w0p0','after-reopen');before=geometry()
                choose(1,'Dracula');assert preferences()['workspace\tw0']['background']=='#282a36'
                choose(1,'Latte');assert preferences()['workspace\tw0']['background']=='#eff1f5'
                assert geometry()==before;alive()
                chrome_action(cmd,window,'Change appearance');cmd('windowsize',window,720,480);time.sleep(.15)
                key('Down');key('Down');capture('gallery-narrow');assert geometry()==before
                cmd('windowsize',window,1240,760);time.sleep(.15);key('Escape')
                choose(1,'Dracula');choose(2,'Latte')
                assert preferences()['terminal\tw0p0']['background']=='#eff1f5'
                assert preferences()['workspace\tw0']['background']=='#282a36'
                assert preferences()['application']['background']=='#15191f','old application preference was overwritten'
                # Runtime color replies remain specific to the terminal exception.
                (root/'color-query.py').write_text("import os,sys,termios,tty,select,time\nfrom pathlib import Path\nold=termios.tcgetattr(0)\ntry:\n tty.setraw(0)\n os.write(1,b'\\x1b[?996n')\n data=b''; deadline=time.monotonic()+3\n while time.monotonic()<deadline:\n  if select.select([0],[],[],.1)[0]:\n   data+=os.read(0,1024)\n   if data.endswith(b'n'): break\n Path(sys.argv[1]).write_bytes(data)\nfinally: termios.tcsetattr(0,termios.TCSANOW,old)\n")
                def query(name,expected):
                    cmd('type','--clearmodifiers','--window',window,'--delay',10,'--',f'python3 color-query.py {name}');key('Return')
                    assert wait_until(lambda:(root/name).exists() and (root/name).read_bytes())==expected
                query('scheme-light',b'\x1b[?997;2n')
                if pane_count>1:click(900,200);query('scheme-dark',b'\x1b[?997;1n');click(300,200)
                # The pane menu's reset action clears only this exception; Cancel restores it.
                choose(2,'Latte',apply=False);click(280,667);capture('terminal-inherit-preview');key('Escape')
                assert preferences()['terminal\tw0p0']['background']=='#eff1f5'
                choose(1,'Dawn',apply=False);capture('workspace-with-exception');key('Escape')
                assert preferences()['workspace\tw0']['background']=='#282a36'
                key('ctrl+shift+o');time.sleep(.25);mixed=capture('mixed-follow')
                assert pixel(mixed,50,300)[0]>200,'terminal exception absent from overview'
                if pane_count>1:assert pixel(mixed,343,300)[0]<80,'workspace palette absent from sibling'
                assert 35<pixel(mixed,28,160)[0]<70,'chrome followed the terminal exception'
                key('Escape');time.sleep(.25)
                if pane_count>1:
                    chrome_workspace(click,1);choose(1,'Mocha');chrome_workspace(click,0);before=geometry()
                # Use-default reset, explicit close and keyboard Cancel retain the existing transaction.
                chrome_action(cmd,window,'Change appearance');click(280,667);key('Escape')
                assert preferences()['workspace\tw0']['background']=='#282a36'
                verify('w0p0','after-scoped-themes');alive()
                close_window(window,env);assert viewer.wait(timeout=5)==0
                if pane_count>1:
                    def catalog_move(identity,destination,position):
                        subprocess.run(['python3',env['WORKBENCH_CATALOG_SCRIPT'],'--state-dir',str(state),'--runtime',runtime,'--cwd',str(root),'--command','terminal.move','--id',identity,'--to',destination,'--position',str(position)],env=env,stdout=log,stderr=log,check=True,timeout=10)
                    catalog_move('w0p0','w1',0);catalog_move('w0p1','w1',1)
                    viewer,window=start();key('ctrl+shift+o');time.sleep(.25);moved=capture('mixed-moved')
                    assert pixel(moved,50,580)[0]>200 and pixel(moved,343,580)[0]<50
                    key('Escape');time.sleep(.25);chrome_workspace(click,1);verify('w0p0','after-theme-move');alive()
                    close_window(window,env);assert viewer.wait(timeout=5)==0
                    catalog_move('w0p0','w0',0);catalog_move('w0p1','w0',1)
                viewer,window=start();verify('w0p0','after-scoped-reopen')
                assert preferences()['terminal\tw0p0']['background']=='#eff1f5'
                close_window(window,env);assert viewer.wait(timeout=5)==0
                # User-supplied Ghostty colors use the same loader and remain a selectable preview.
                imported=root/'imported.theme'; imported.write_text('background=#202060\nforeground=#eeeeee\npalette=2=#00cc66\nfont-family=Ignored\n')
                env['WORKBENCH_THEME']=str(imported)
                viewer,window=start(); chrome_action(cmd,window,'Change appearance')
                cmd('type','--window',window,'--delay',20,'--','Imported')
                capture('imported-theme'); key('Escape')
                verify('w0p0','after-import'); close_window(window,env); assert viewer.wait(timeout=5)==0; alive()
                # Old global preferences migrate without rewriting their source file.
                env.pop('WORKBENCH_THEME'); scoped=(state/'appearance.state').read_bytes()
                (state/'appearance.state').unlink()
                legacy=state/'appearance.conf'; legacy.write_text('background=#f4f1eb\nforeground=#303841\n')
                viewer,window=start(); chrome_action(cmd,window,'Change appearance')
                assert pixel(capture('legacy-migration'),210,180)[0]>200
                assert not (state/'appearance.state').exists()
                key('Return')
                assert preferences()['application']['background']=='#f4f1eb'
                assert preferences()['default']['background']=='#f4f1eb'
                assert legacy.read_text()=='background=#f4f1eb\nforeground=#303841\n'
                close_window(window,env); assert viewer.wait(timeout=5)==0; alive()
                (state/'appearance.state').write_bytes(scoped)
                # A malformed scoped file also fails before attaching and stays untouched.
                bad=b'workbench-appearance-v1\nfollow\t2\n'
                (state/'appearance.state').write_bytes(bad)
                failed=subprocess.run([binary],env=env,cwd=root,stdout=log,stderr=log,timeout=5)
                assert failed.returncode==1 and (state/'appearance.state').read_bytes()==bad; alive()
                (state/'appearance.state').write_bytes(scoped)
                env['WORKBENCH_THEME']=str(imported)
                imported.write_text('background=not-a-color\n')
                failed=subprocess.run([binary],env=env,cwd=root,stdout=log,stderr=log,timeout=5)
                assert failed.returncode==1; alive()
                print(f'PASS ({pane_count} panes): workspace-first picker, steady surfaces, matching chrome, terminal exceptions/reset, cancellation, failed saves, reopen, import/legacy/corrupt isolation, query replies and stable PID/socket/grid; idle overview CPU Dawn={day_cpu}% Night={night_cpu}% (Xvfb/llvmpipe)')
            finally:
                if sys.exc_info()[0] and viewer is not None and viewer.poll() is None:
                    capture('failure')
                for process in reversed(processes):
                    if process.poll() is None: process.terminate(); process.wait(timeout=5)
                if sys.exc_info()[0]:
                    connection,replay,_=attach(state/'w0p0.sock'); print(repr(replay[-1800:]),file=sys.stderr); connection.close()
                for path in state.glob('*.sock'): subprocess.run([runtime,'stop',str(path)],capture_output=True,timeout=12)
                if sys.exc_info()[0]: log.seek(0); print(log.read()[-5000:],file=sys.stderr)
if __name__=='__main__': main()
