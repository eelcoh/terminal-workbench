#!/usr/bin/env python3
"""Native Wayland input/resize/reattach, confined to a private nested Niri."""
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import struct
import sys
import tempfile
import time

from session_smoke import attach, send, wait_until

ROOT = Path(__file__).resolve().parent.parent
READER = r"""
import os, tty, termios, select
from pathlib import Path
saved = termios.tcgetattr(0)
tty.setraw(0)
Path('reader-pid').write_text(str(os.getpid()))
os.write(1, b'\x1b[?2004hWayland input ready\r\n')
flags=0
pointer_drawn=""
try:
    while True:
        requested=int(Path('keyboard-flags').read_text()) if Path('keyboard-flags').exists() else 0
        if requested!=flags:
            if flags:os.write(1,b'\x1b[<u')
            flags=requested
            if flags:os.write(1,b'\x1b[>'+str(flags).encode()+b'u')
        pointer_requested=Path('pointer-output').read_text() if Path('pointer-output').exists() else ''
        if pointer_requested and pointer_requested!=pointer_drawn:
            os.write(1,b'\x1b[?1000l\x1b[?1006l')
            if pointer_requested.startswith('tracking:'):os.write(1,b'\x1b[?1000h\x1b[?1006h')
            os.write(1,'\x1b[2J\x1b[Hclipboard sample\r\ncafé λ\r\n'.encode())
            pointer_drawn=pointer_requested
            Path('pointer-ready').write_text(pointer_drawn)
        Path('flags-ack').write_text(str(flags))
        Path('size').write_text(str(termios.tcgetwinsize(0)))
        if select.select([0], [], [], .1)[0]:
            with open('received', 'ab') as output:
                output.write(os.read(0, 65536))
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
"""


def stop(process):
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def main():
    runtime, viewer = [Path(arg).resolve() for arg in sys.argv[1:3]]
    layouts = os.environ.get('WORKBENCH_TEST_LAYOUTS') == '1'
    display_test = os.environ.get('WORKBENCH_TEST_DISPLAY') == '1'
    pointer_test = os.environ.get('WORKBENCH_TEST_POINTER') == '1'
    report = ROOT / ('.local/artifacts/wayland-pointer' if pointer_test else '.local/artifacts/wayland-display' if display_test else '.local/artifacts/keyboard-layouts' if layouts else '.local/artifacts/wayland')
    report.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='wb-wayland-') as directory:
        root = Path(directory)
        processes, logs = [], []
        sock = root / 'test.sock'
        rd = wr = None
        def spawn(args, **kwargs):
            process = subprocess.Popen(args, **kwargs)
            processes.append(process)
            return process
        def logfile(name):
            handle = (root/name).open('w')
            logs.append(handle)
            return handle
        try:
            log = logfile('viewer.log')
            niri_log = logfile('niri.log')
            xlog = logfile('xvfb.log')
            nested = dict(os.environ, WINIT_UNIX_BACKEND='x11', LIBGL_ALWAYS_SOFTWARE='1')
            for name in ('WAYLAND_DISPLAY', 'NIRI_SOCKET', 'WORKBENCH_SOCKET',
                         'WORKBENCH_CONFIG', 'WORKBENCH_SHORTCUTS',
                         'WORKBENCH_CATALOG_SCRIPT', 'WORKBENCH_WINDOW_SLOT_FD',
                         'WORKBENCH_RENDER_SCALE', 'WORKBENCH_RENDER_SCALE_FILE',
                         'WORKBENCH_TEST_KEYBOARD_LAYOUT', 'WORKBENCH_TEST_KEYBOARD_VARIANT'):
                nested.pop(name, None)
            rd, wr = os.pipe()
            spawn(['Xvfb', '-displayfd', str(wr), '-screen', '0', '2560x1600x24' if display_test or pointer_test else '1440x1000x24',
                   '-noreset', '-nolisten', 'tcp'], pass_fds=(wr,), stdout=xlog, stderr=xlog)
            os.close(wr); wr = None
            assert select.select([rd], [], [], 5)[0], 'Xvfb startup timed out'
            with os.fdopen(rd) as pipe:
                nested['DISPLAY'] = ':' + pipe.readline().strip()
            rd = None
            (root/'niri.kdl').write_text(
                'hotkey-overlay { skip-at-startup; }\nanimations { off; }\n'
                'layout { default-column-width { proportion 0.9; }; }\n'+('prefer-no-csd\n' if pointer_test else ''))
            # No --session: never import environment into the user's desktop.
            compositor = spawn(['niri', '-c', str(root/'niri.kdl')],
                               env=nested, stdout=niri_log, stderr=niri_log)
            def endpoints():
                assert compositor.poll() is None, 'Nested Niri exited'
                text = (root/'niri.log').read_text()
                display = re.search(r'Wayland socket: (\S+)', text)
                ipc = re.search(r'IPC listening on: (\S+)', text)
                return (display[1], ipc[1]) if display and ipc else None
            display, ipc = wait_until(endpoints, timeout=15)
            env = dict(nested, WAYLAND_DISPLAY=display, NIRI_SOCKET=ipc)
            env.pop('DISPLAY', None)  # A successful viewer cannot use XWayland.
            if display_test or pointer_test:
                def xrun(*args):
                    return subprocess.run(['xdotool',*args],env=nested,capture_output=True,check=True,timeout=10)
                parent=wait_until(lambda:xrun('search','--onlyvisible','--pid',str(compositor.pid)).stdout.decode().strip())
                xrun('windowsize','--sync',parent.splitlines()[0],'1920','1200')
                time.sleep(.3)
            def run(*args):
                return subprocess.run(args, env=env, capture_output=True, check=True, timeout=10)
            def action(*args):
                return run('niri', 'msg', 'action', *args)
            def windows():
                return json.loads(run('niri', 'msg', '-j', 'windows').stdout)
            protocol = ROOT/'tests/fixtures/virtual-keyboard-unstable-v1.xml'
            run('wayland-scanner', 'client-header', str(protocol), str(root/'virtual-keyboard.h'))
            run('wayland-scanner', 'private-code', str(protocol), str(root/'virtual-keyboard.c'))
            flags = run('pkg-config', '--cflags', '--libs', 'wayland-client', 'xkbcommon').stdout.decode().split()
            run('cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(root),
                str(ROOT/'tests/wayland_keyboard.c'), str(root/'virtual-keyboard.c'),
                '-o', str(root/'keyboard'), *flags)
            def text(value):
                run(str(root/'keyboard'), 'text', value)
            def key(value, *mods):
                run(str(root/'keyboard'), 'key', value, *mods)
            (root/'reader.py').write_text(READER)
            spawn([str(runtime), str(sock), '/bin/sh'], cwd=root, env=env, stdout=log, stderr=log)
            wait_until(sock.exists)
            connection, _, _ = attach(sock)
            send(connection, 16, ('cd '+str(root)+'; python3 '+str(root/'reader.py')+'\n').encode())
            connection.close()
            wait_until((root/'reader-pid').exists)
            (root/'catalog').write_text('workbench-catalog-v3\nW\tmain\tWayland\n'
                                       'T\tmain\tsession\tShell\t'+str(sock)+'\n')
            env.update(WORKBENCH_WORKSPACES=str(root/'catalog'),
                       WORKBENCH_WINDOW_STATE=str(root/'window'))
            def launch():
                process = spawn([str(viewer)], cwd=root, env=env, stdout=log, stderr=log)
                def ready():
                    assert process.poll() is None, 'Viewer exited before mapping'
                    return windows()
                wait_until(ready, timeout=15)
                time.sleep(1)
                return process
            process = launch()
            expected = b'AaBb repeated aaaaa 1234 /exit !?\r'
            text(expected[:-1].decode()); key('Return')
            def received():
                return (root/'received').read_bytes() if (root/'received').exists() else b''
            wait_until(lambda: received() == expected)
            print('PASS: native Wayland exact text and Enter order', flush=True)
            if layouts:
                def set_flags(flags):
                    pending=root/'keyboard-flags-next'
                    pending.write_text(str(flags));pending.replace(root/'keyboard-flags')
                    wait_until(lambda:(root/'flags-ack').read_text()==str(flags))
                    time.sleep(.2)
                env['WORKBENCH_TEST_KEYBOARD_LAYOUT']='de'
                text('z y Z Y');expected+=b'z y Z Y'
                wait_until(lambda: received()==expected)
                key('z','Control');expected+=b'\x1a'
                try:wait_until(lambda: received()==expected)
                except AssertionError:raise AssertionError(('German Ctrl-Z',received(),expected))
                key('z','Mod1');expected+=b'\x1bz'
                wait_until(lambda: received()==expected)
                print('PASS: German text, Ctrl-Z and Alt-Z follow the active layout',flush=True)
                for flags in (1,3,31):
                    # The fixture's acknowledgement precedes viewer VT parsing.
                    set_flags(flags)
                    before=len(received());key('z','Control');time.sleep(.2)
                    encoded=received()[before:]
                    events=re.findall(rb'\x1b\[([0-9]+)(?::[0-9:]+)?;([0-9]+)(?::([0-9]+))?(?:;[0-9:]+)?u',encoded)
                    assert events and all(cp==b'122' and mods==b'5' for cp,mods,_ in events),(flags,encoded)
                    assert any(action!=b'3' for _,_,action in events)
                    if flags in (3,31):assert any(action==b'3' for _,_,action in events),(flags,encoded)
                    expected+=encoded
                set_flags(0)
                env['WORKBENCH_TEST_KEYBOARD_LAYOUT']='fr'
                text('azerty');expected+=b'azerty'
                key('a','Control');expected+=b'\x01'
                wait_until(lambda:received()==expected)
                env['WORKBENCH_TEST_KEYBOARD_LAYOUT']='us'
                env['WORKBENCH_TEST_KEYBOARD_VARIANT']='intl'
                run(str(root/'keyboard'),'sequence','dead_acute e')
                expected+='é'.encode()
                wait_until(lambda:received()==expected)
                env.pop('WORKBENCH_TEST_KEYBOARD_VARIANT')
                env.pop('WORKBENCH_TEST_KEYBOARD_LAYOUT')
                print('PASS: German Ctrl-Z Kitty flags 1/3/31, French text/Ctrl-A and US-international dead-key composition',flush=True)
            clip = 'Wayland clipboard café λ\nsecond line'
            spawn(['wl-copy', '--foreground', '--', clip], env=env,
                  stdin=subprocess.DEVNULL, stdout=log, stderr=log)
            def clipboard_ready():
                reply=subprocess.run(['wl-paste','--no-newline'],env=env,capture_output=True,timeout=5)
                return reply.returncode==0 and reply.stdout==clip.encode()
            wait_until(clipboard_ready)
            key('v', 'Control', 'Shift')
            expected += b'\x1b[200~'+clip.encode()+b'\x1b[201~'
            wait_until(lambda: received() == expected)
            print('PASS: native Unicode bracketed clipboard paste', flush=True)
            if pointer_test:
                def place():
                    action('move-window-to-floating')
                    action('set-window-width','800');action('set-window-height','420')
                    action('move-floating-window','--x','100','--y','100')
                    time.sleep(.5)
                def point(x,y,scale):
                    layout=windows()[0]['layout']
                    origin=layout['tile_pos_in_workspace_view'];offset=layout['window_offset_in_tile']
                    xrun('mousemove','--window',parent.splitlines()[0],str(round((origin[0]+offset[0]+x)*scale)),str(round((origin[1]+offset[1]+y)*scale)))
                    time.sleep(.08)
                def drag(start,end,scale,row=0):
                    point(58+start*7+(5 if start>end else 2),86+row*16+8,scale)
                    xrun('mousedown','1');time.sleep(.08)
                    point(58+end*7+(2 if start>end else 5),86+row*16+8,scale)
                    xrun('mouseup','1');time.sleep(.15)
                def selected():
                    reply=run('wl-paste','--no-newline','--type','text/plain;charset=utf-8')
                    return reply.stdout.decode()
                def seed_clipboard(value):
                    spawn(['wl-copy','--foreground','--',value],env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=log)
                    def ready():
                        reply=subprocess.run(['wl-paste','--no-newline'],env=env,capture_output=True,timeout=5)
                        return reply.returncode==0 and reply.stdout==value.encode()
                    wait_until(ready)
                def copy_expected(value,tag):
                    key('c','Control','Shift')
                    actual=selected()
                    if actual!=value:
                        action('screenshot-screen','--path',str(root/('failed-'+tag+'.png')),'--show-pointer','true')
                    assert actual==value,(tag,actual,value)
                def draw(tag):
                    pending=root/'pointer-output-next';pending.write_text(tag);pending.replace(root/'pointer-output')
                    wait_until(lambda:(root/'pointer-ready').exists() and (root/'pointer-ready').read_text()==tag)
                    time.sleep(.3)
                place()
                for tag,scale,reopen in [('initial',1,False),('fractional',1.5,False),('reopened-fractional',1.5,True)]:
                    run('niri','msg','output','winit','scale',str(scale));time.sleep(.8)
                    if reopen:
                        action('close-window');process.wait(timeout=10);wait_until(lambda:not windows())
                        process=launch();place()
                    draw(tag)
                    # Enter from outside and click without a second motion event.
                    xrun('mousemove','--window',parent.splitlines()[0],'1910','1190');time.sleep(.1)
                    seed_clipboard('forward sentinel');drag(0,8,scale);copy_expected('clipboard',tag+'-forward')
                    seed_clipboard('reverse sentinel');drag(8,0,scale);copy_expected('clipboard',tag+'-reverse')
                    seed_clipboard('unicode sentinel');drag(0,5,scale,row=1);copy_expected('café λ',tag+'-unicode')
                    key('v','Control','Shift');expected+=b'\x1b[200~'+'café λ'.encode()+b'\x1b[201~'
                    wait_until(lambda:received()==expected)
                    draw('tracking:'+tag)
                    point(58+3*7+3,94,scale)
                    xrun('mousedown','1');time.sleep(.08);xrun('mouseup','1');time.sleep(.15)
                    expected+=b'\x1b[<0;4;1M\x1b[<0;4;1m'
                    wait_until(lambda:received()==expected)
                    seed_clipboard('shift sentinel')
                    ready=root/('shift-ready-'+tag)
                    held=spawn([str(root/'keyboard'),'hold','Shift_L'],env=dict(env,WORKBENCH_TEST_KEY_DOWN=str(ready)),
                        stdin=subprocess.PIPE,stdout=log,stderr=log)
                    wait_until(ready.exists);time.sleep(.1)
                    try:drag(0,8,scale)
                    finally:
                        held.stdin.write(b'\n');held.stdin.close();held.wait(timeout=5)
                    time.sleep(.1)
                    copy_expected('clipboard',tag+'-shift')
                    assert received()==expected,'local selection leaked terminal mouse/key input'
                    action('screenshot-window','--path',str(root/('pointer-'+tag+'.png')))
                    print('PASS: Wayland drag/reverse/Unicode copy-paste, SGR mouse and Shift selection',tag,flush=True)
                run('niri','msg','output','winit','scale','1');time.sleep(.5)
                action('move-window-to-tiling');time.sleep(.5)
            if display_test:
                action('move-window-to-floating')
                action('set-window-width','800');action('set-window-height','420')
                time.sleep(.5)
                original_size=(root/'size').read_text()
                original_window=windows()[0]['layout']['window_size']
                pid=(root/'reader-pid').read_text()
                def pixels(path):
                    def header_ready():
                        try:
                            with path.open('rb') as image:header=image.read(24)
                        except FileNotFoundError:return None
                        return header if len(header)==24 else None
                    header=wait_until(header_ready)
                    assert header[:8]==b'\x89PNG\r\n\x1a\n'
                    return struct.unpack('!II',header[16:24])
                baseline=root/'scale-1-before.png'
                action('screenshot-window','--path',str(baseline))
                original_pixels=pixels(baseline)
                observations=[]
                for scale in (1.25,1.5,2,1):
                    key('o','Mod4','Mod1');time.sleep(.2)
                    log_start=(root/'viewer.log').stat().st_size
                    run('niri','msg','output','winit','scale',str(scale))
                    marker=f'Display backing scale updated: {scale:g};'
                    wait_until(lambda:marker in (root/'viewer.log').read_text()[log_start:],timeout=15)
                    time.sleep(.3)
                    overview_capture=root/('overview-scale-'+str(scale)+'.png')
                    action('screenshot-window','--path',str(overview_capture))
                    key('Escape');time.sleep(.3)
                    current=windows()[0]
                    capture=root/('scale-'+str(scale)+'.png')
                    action('screenshot-window','--path',str(capture))
                    grid=(root/'size').read_text()
                    actual_pixels=pixels(capture)
                    observations.append(dict(scale=scale,grid=grid,
                        window=current['layout']['window_size'],pixels=actual_pixels,
                        overview_pixels=pixels(overview_capture),backing_update=marker))
                    assert grid==original_size,(scale,original_size,grid)
                    assert current['layout']['window_size']==original_window,(scale,current)
                    # Compositor captures include decoration/shadow rounding.
                    for logical,actual in zip(original_pixels,actual_pixels):
                        assert abs(actual-logical*scale)<=2,(scale,original_pixels,actual_pixels)
                    assert pixels(overview_capture)==actual_pixels
                    text('scale'+str(scale));expected+=('scale'+str(scale)).encode()
                    wait_until(lambda:received()==expected)
                    assert (root/'reader-pid').read_text()==pid
                (root/'display.json').write_text(json.dumps(observations,indent=2))
                print('PASS: 1→1.25→1.5→2→1 backing pixels, Overview, logical grid, shell and input',flush=True)
                action('move-window-to-tiling');time.sleep(.5)
            old = (root/'size').read_text()
            action('set-column-width', '50%')
            wait_until(lambda: (root/'size').read_text() not in ('', old))
            print('PASS: compositor resize reaches PTY', flush=True)
            key('o', 'Mod4', 'Mod1'); time.sleep(.3)
            action('screenshot-window', '--path', str(root/'overview.png'))
            key('Escape'); time.sleep(.3)
            pid = (root/'reader-pid').read_text()
            action('close-window'); process.wait(timeout=10)
            wait_until(lambda: not windows())
            process = launch()
            text('reattach')
            wait_until(lambda: received() == expected+b'reattach')
            assert (root/'reader-pid').read_text() == pid
            print('PASS: close/reopen keeps the PTY process and input', flush=True)
            action('screenshot-window', '--path', str(root/'working.png'))
            action('close-window'); process.wait(timeout=10)
            assert 'GLFW - Wayland' in (root/'viewer.log').read_text()
            print('PASS: Wayland backend confirmed with DISPLAY unset', flush=True)
        finally:
            if sock.exists():
                subprocess.run([str(runtime), 'stop', str(sock)],
                               capture_output=True, timeout=15)
            # Includes each viewer, clipboard owner, compositor and private X server.
            for process in reversed(processes):
                stop(process)
            for descriptor in (rd, wr):
                if descriptor is not None:
                    os.close(descriptor)
            for handle in logs:
                handle.close()
            for path in root.iterdir():
                if path.is_file() and (path.suffix in ('.log', '.png', '.json') or path.name == 'received'):
                    shutil.copy2(path, report/path.name)
    return 0


if __name__ == '__main__':
    sys.exit(main())
