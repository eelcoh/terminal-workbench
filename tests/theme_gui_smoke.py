#!/usr/bin/env python3
"""Live theme preview, save/cancel, parser errors, session continuity and UI samples."""
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

def pixel(path, px, py):
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
        if y == py: return tuple(row[px*channels:px*channels+3])
        prior=row
    raise AssertionError('Pixel outside screenshot')

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
                def cpu_sample():
                    begin=time.monotonic()
                    def ticks(): return sum(map(int,Path(f'/proc/{viewer.pid}/stat').read_text().split(') ',1)[1].split()[11:13]))
                    baseline=ticks(); time.sleep(2)
                    return round(100*(ticks()-baseline)/os.sysconf('SC_CLK_TCK')/(time.monotonic()-begin),1)
                before=geometry()
                click(84,720); capture('appearance-night')
                click(615,244); light=capture('appearance-dawn')
                # Background swatch lives inside the appearance dialog in both themes.
                assert pixel(light,200,180)[0]>200
                assert geometry()==before; alive()
                cmd('type','--window',window,'--delay',20,'--','modal-text-must-not-reach-shell')
                capture('gallery-no-match'); key('Return')
                assert not (state/'appearance.conf').exists()
                key('ctrl+a'); click(615,244)
                (state/'appearance.conf').mkdir()
                key('Return'); time.sleep(.1)
                assert (state/'appearance.conf').is_dir(), 'failed save replaced metadata'
                capture('save-error'); (state/'appearance.conf').rmdir()
                key('Escape'); time.sleep(.1)
                # Cancellation returns to the exact previous terminal/UI palette.
                click(84,720)
                assert pixel(capture('cancelled'),200,180)[0]<60
                key('Escape')
                key('ctrl+shift+o'); time.sleep(.25); before=geometry()
                click(292,74); key('Right'); key('Return'); time.sleep(.15)
                saved=wait_until(lambda:(state/'appearance.conf').exists() and (state/'appearance.conf').read_text())
                assert 'background = #f4f1eb' in saved
                assert geometry()==before; alive()
                capture('overview-dawn')
                day_cpu=cpu_sample()
                click(292,74); key('Left'); key('Return'); time.sleep(.15)
                capture('overview-night')
                night_cpu=cpu_sample()
                key('Escape'); time.sleep(.25); verify('w0p0','after-theme')
                capture('working-night'); key('ctrl+u') # Raylib's F12 debug key also reaches a normal terminal.
                click(1196,16); assert viewer.wait(timeout=5)==0; alive()
                viewer,window=start()
                assert 'background = #15191f' in (state/'appearance.conf').read_text()
                verify('w0p0','after-reopen')
                click(1196,16); assert viewer.wait(timeout=5)==0
                # Gallery filtering and selection use real bundled Ghostty colors.
                viewer,window=start(); before=geometry(); click(84,720)
                click(380,156); capture('gallery-light'); assert geometry()==before; alive()
                click(308,156); capture('gallery-dark')
                cmd('type','--window',window,'--delay',20,'--','Dracula')
                capture('gallery-dracula'); key('Return')
                assert 'background = #282a36' in (state/'appearance.conf').read_text()
                assert geometry()==before; alive()
                click(84,720); click(380,156)
                cmd('type','--window',window,'--delay',20,'--','Latte')
                capture('gallery-latte'); key('Return')
                assert 'background = #eff1f5' in (state/'appearance.conf').read_text()
                assert geometry()==before; alive()
                click(84,720); cmd('windowsize',window,720,480); time.sleep(.15)
                key('Down'); key('Down'); key('Down'); capture('gallery-narrow')
                assert geometry()==before, 'narrow palette picker resized PTYs'
                cmd('windowsize',window,1240,760); time.sleep(.15); key('Escape')
                assert geometry()==before; alive()
                click(1196,16); assert viewer.wait(timeout=5)==0
                viewer,window=start()
                assert 'background = #eff1f5' in (state/'appearance.conf').read_text()
                verify('w0p0','after-gallery-reopen'); click(1196,16); assert viewer.wait(timeout=5)==0
                # User-supplied Ghostty colors use the same loader and remain a selectable preview.
                imported=root/'imported.theme'; imported.write_text('background=#202060\nforeground=#eeeeee\npalette=2=#00cc66\nfont-family=Ignored\n')
                env['WORKBENCH_THEME']=str(imported)
                viewer,window=start(); click(84,720)
                capture('imported-theme'); key('Escape')
                verify('w0p0','after-import'); click(1196,16); assert viewer.wait(timeout=5)==0; alive()
                imported.write_text('background=not-a-color\n')
                failed=subprocess.run([binary],env=env,cwd=root,stdout=log,stderr=log,timeout=5)
                assert failed.returncode==1; alive()
                print(f'PASS ({pane_count} panes): dark/light live preview, cancel, save failure, persistence, imported colors, malformed-theme preservation, unchanged PIDs/socket/geometry and modal input isolation; gallery search/filters, Dracula/Latte apply/reopen, no-match and narrow-window continuity; idle overview CPU Dawn={day_cpu}% Night={night_cpu}% (Xvfb/llvmpipe)')
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
