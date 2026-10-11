#!/usr/bin/env python3
"""Spatial overview: live hidden screens, geometry preservation and navigation."""
from pathlib import Path
import fcntl
import json
import os
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import termios
import time
import zlib
from session_smoke import attach, send, wait_until

ROOT = Path(__file__).resolve().parent.parent

def red_pixels(path, box, centroid=False):
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
    channels, offset, count, weighted_y = (4 if color == 6 else 3), 0, 0, 0
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
        if box[1] <= y < box[3]:
            for x in range(box[0], box[2]):
                r, g, b = row[x * channels:x * channels + 3]
                red = r > 100 and r > 1.7 * g and r > 1.7 * b
                count += red; weighted_y += red*y
        prior = row
    return weighted_y/count if centroid and count else count

def main():
    runtime, viewer_binary = [str(Path(arg).resolve()) for arg in sys.argv[1:3]]
    workspaces=int(os.environ.get('WORKBENCH_TEST_OVERVIEW_WORKSPACES','3')); panes=int(os.environ.get('WORKBENCH_TEST_OVERVIEW_PANES','5'))
    assert 3<=workspaces<=8 and 5<=panes<=8
    with tempfile.TemporaryDirectory(prefix='wb-expose-') as directory:
        root = Path(directory); state = root / 'sessions'; state.mkdir()
        metadata = {'version': 1, 'workspaces': [{'id': f'w{w}', 'name': f'Workspace {w}',
            'terminals': [{'id': f'w{w}p{p}', 'name': f'Terminal {p}'} for p in range(panes)]} for w in range(workspaces)]}
        (state / 'workspaces.json').write_text(json.dumps(metadata))
        processes = []
        environment = dict(os.environ, SHELL='/bin/sh', LIBGL_ALWAYS_SOFTWARE='1',
            WORKBENCH_WORKSPACES=str(state / 'workspaces.tsv'))
        environment.pop('WAYLAND_DISPLAY', None); environment.pop('WORKBENCH_SOCKET', None)
        with (root / 'test.log').open('w+') as log:
            try:
                subprocess.run(['python3', str(ROOT / 'scripts/workspace_catalog.py'), '--state-dir', str(state), '--runtime', runtime, '--cwd', str(root), '--command', 'session.recover'], env=environment, stdout=log, stderr=log, check=True, timeout=20)
                pids = {}
                for w in range(workspaces):
                    for p in range(panes):
                        identity = f'w{w}p{p}'
                        connection, _, _ = attach(state / f'{identity}.sock')
                        send(connection, 16, f"keep={identity}; echo $$ > {identity}.pid; printf '\\033[32mWORKSPACE {w} TERMINAL {p}\\033[0m\\n'\n".encode())
                        wait_until(lambda: (root / f'{identity}.pid').exists())
                        pids[identity] = int((root / f'{identity}.pid').read_text())
                        if identity == 'w1p0':
                            send(connection, 16, b"(while [ ! -e reveal ]; do sleep .05; done; printf '\\033[48;2;200;30;30m                                                  \\033[0m\\n'; touch changed) &\n")
                        connection.close()
                        time.sleep(0.04)
                read_fd, write_fd = os.pipe()
                xvfb = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1440x1000x24', '-noreset', '-nolisten', 'tcp'], pass_fds=(write_fd,), stdout=log, stderr=log)
                processes.append(xvfb); os.close(write_fd)
                assert select.select([read_fd], [], [], 5)[0]
                environment['DISPLAY'] = ':' + os.read(read_fd, 32).decode().strip(); os.close(read_fd)
                viewer = subprocess.Popen([viewer_binary], env=environment, cwd=root, stdout=log, stderr=log)
                processes.append(viewer)
                def command(*args):
                    result = subprocess.run(['xdotool', *map(str, args)], env=environment, capture_output=True, text=True, timeout=5)
                    result.check_returncode(); return result.stdout.strip()
                def find():
                    assert viewer.poll() is None
                    result = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(viewer.pid)], env=environment, capture_output=True, text=True)
                    return result.stdout.strip().splitlines()[-1] if result.returncode == 0 else None
                window = wait_until(find, timeout=10); command('windowfocus', window); time.sleep(0.6)
                def key(name):
                    command('keydown', '--window', window, name); time.sleep(.06)
                    command('keyup', '--window', window, name); time.sleep(.08)
                def click(x, y):
                    command('mousemove', '--window', window, x, y); command('mousedown', 1); time.sleep(.06)
                    command('mouseup', 1); time.sleep(.08)
                def fast_key(name):
                    command('keydown', '--window', window, name); time.sleep(.025)
                    command('keyup', '--window', window, name); time.sleep(.025)
                def fast_click(x, y):
                    command('mousemove', '--window', window, x, y); command('mousedown', 1); time.sleep(.025)
                    command('mouseup', 1); time.sleep(.025)
                def text(value): command('type', '--clearmodifiers', '--window', window, '--delay', 10, '--', value)
                def verify(identity, name):
                    text(f'printf "%s:%s" "$$" "$keep" > {name}'); key('Return')
                    actual = wait_until(lambda: (root / name).exists() and (root / name).read_text())
                    assert actual == f'{pids[identity]}:{identity}', (identity, actual)
                def geometry():
                    result = {}
                    for identity, pid in pids.items():
                        fd = os.open(f'/proc/{pid}/fd/0', os.O_RDONLY | os.O_NONBLOCK)
                        try: result[identity] = fcntl.ioctl(fd, termios.TIOCGWINSZ, bytes(8))
                        finally: os.close(fd)
                    return result
                artifacts = ROOT / '.local/test-results'; artifacts.mkdir(parents=True, exist_ok=True)
                def capture(name):
                    key('F12'); time.sleep(.15)
                    path = sorted(root.glob('screenshot*.png'))[-1]
                    destination = artifacts / name; shutil.copyfile(path, destination); return destination
                before = geometry()
                fast_click(522, 16)
                fast_key('F12')
                time.sleep(.05)
                shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1], artifacts / 'overview-opening.png')
                time.sleep(.2)
                first = capture('expose-before.png')
                (root / 'reveal').touch(); wait_until(lambda: (root / 'changed').exists()); time.sleep(.3)
                live = capture('expose-live.png')
                box = (44, 488, 305, 669)  # Hidden workspace's first terminal preview.
                assert red_pixels(live, box) > red_pixels(first, box) + 20, 'hidden terminal preview did not update'
                assert geometry() == before, 'opening overview changed PTY geometry'
                log.flush()
                assert (root / 'test.log').read_text().count('Texture loaded successfully (384x240') == 8, 'offscreen previews allocated or live updates reallocated'
                # One notch moves only 40 pixels, preserving a partial workspace
                # rather than snapping to the next row. Trackpad fractions use the
                # same path; callback and pure tests exercise sub-notch values.
                command('mousemove','--window',window,180,290); command('click',5); time.sleep(.25)
                partial=capture('overview-scroll-partial.png')
                scroll_box=(44,300,305,700)
                displacement=red_pixels(live,scroll_box,True)-red_pixels(partial,scroll_box,True)
                assert 35<displacement<45, ('overview jumped instead of scrolling fractionally',displacement)
                command('click',4); time.sleep(.25)
                reversed_view=capture('overview-scroll-reversed.png')
                assert abs(red_pixels(live,scroll_box,True)-red_pixels(reversed_view,scroll_box,True))<2
                assert geometry()==before, 'overview scrolling resized a terminal'
                reduced = os.environ.get('WORKBENCH_REDUCED_MOTION') == '1'
                assert ('Texture loaded successfully (1240x760' in (root / 'test.log').read_text()) != reduced
                if not reduced:
                    # Pending navigation must not commit if the zoom is reversed.
                    text('Workspace 2'); fast_key('Return'); fast_click(522, 16)
                    text('missing-terminal'); fast_key('Return'); fast_key('Escape')
                    # Immediate typing completes closing without losing queued text.
                    verify('w0p0', 'cancel-safe')
                    key('ctrl+shift+o'); time.sleep(.2); fast_key('Escape'); fast_key('F12')
                    time.sleep(.05)
                    shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1], artifacts / 'overview-closing.png')
                    time.sleep(.2)
                    verify('w0p0', 'after-zoom')
                else:
                    text('missing-terminal'); key('Return'); key('Escape')
                    verify('w0p0', 'cancel-safe')
                key('ctrl+shift+o')
                for _ in range(4): key('Right')
                key('Return'); verify('w0p4', 'horizontal-jump')
                key('ctrl+shift+o'); key('Down'); key('Return'); verify('w1p4', 'workspace-jump')
                key('ctrl+shift+o'); text('Workspace 2'); key('Right'); key('Right'); key('Return')
                verify('w2p2', 'filtered-jump')
                key('ctrl+shift+o'); text('Workspace 0')
                command('mousemove', '--window', window, 180, 290)
                command('keydown', 'Shift_L'); command('click', '--repeat', '8', '--delay', '40', 5); command('keyup', 'Shift_L'); time.sleep(.2)
                click(170, 300); verify('w0p1', 'shift-scroll')
                key('ctrl+shift+o')
                command('mousemove', '--window', window, 180, 290); command('click', '--repeat', '8', '--delay', '40', 5); time.sleep(.3)
                click(170, 300); verify('w1p0', 'vertical-scroll')
                key('ctrl+shift+o'); before = geometry()
                command('windowsize', window, 800, 300); time.sleep(.3)
                assert geometry() == before, 'overview window resize changed PTY geometry'
                key('Down'); key('Return')
                command('windowsize', window, 1240, 760); time.sleep(.3)
                verify('w2p0', 'small-window-jump')
                click(1196, 16); assert viewer.wait(timeout=5) == 0
                for pid in pids.values(): os.kill(pid, 0)
                print(('PASS (reduced motion): ' if reduced else 'PASS (animated): ') + 'spatial grouping, live never-opened previews, preserved geometry/PIDs, modal isolation, horizontal/vertical keyboard navigation, filters, both scroll axes, minimum-window navigation and detach' + ('; reversed pending jump and immediate transition typing' if not reduced else ''))
            finally:
                for process in reversed(processes):
                    if process.poll() is None: process.terminate(); process.wait(timeout=5)
                if sys.exc_info()[0]:
                    connection, replay, _ = attach(state / 'w0p0.sock')
                    print(repr(bytes(replay[-1500:])), file=sys.stderr)
                    connection.close()
                for path in state.glob('*.sock'):
                    subprocess.run([runtime, 'stop', str(path)], capture_output=True, timeout=12)
                if sys.exc_info()[0]:
                    log.seek(0); print(log.read()[-5000:], file=sys.stderr)

if __name__ == '__main__': main()
