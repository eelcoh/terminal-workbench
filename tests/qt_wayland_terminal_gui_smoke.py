"""Qt/Ghostty native Wayland acceptance in a private Niri + Xvfb session."""
import json
import os
from pathlib import Path
import re
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, send, wait_until

ROOT = Path(__file__).resolve().parent.parent
ARTIFACTS = ROOT / ".local/artifacts/qt-wayland-terminal"
READER = r"""
import os,tty,termios,select
from pathlib import Path
saved=termios.tcgetattr(0);tty.setraw(0)
try:
 Path('reader-pid').write_text(str(os.getpid()))
 os.write(1,b''.join(b'history-%03d\r\n'%i for i in range(80)))
 os.write(1,'\x1b[2J\x1b[Hcopy sample\r\ncafé λ\r\nQT-SNAPSHOT\r\n\x1b[32mgreen text\x1b[0m\x1b[?2004h'.encode())
 Path('reader-ready').touch()
 updated=False
 while True:
  if select.select([0],[],[],.02)[0]:
   with open('received','ab') as f:f.write(os.read(0,65536))
  Path('size').write_text(str(termios.tcgetwinsize(0)))
  if Path('live').exists() and not updated:
   os.write(1,b'\x1b[6;1HLIVE-OUTPUT');updated=True
finally:termios.tcsetattr(0,termios.TCSANOW,saved)
"""


def stop(process):
    if process.poll() is None:process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)


def main():
    runtime,viewer=[Path(arg).resolve() for arg in sys.argv[1:3]]
    ARTIFACTS.mkdir(parents=True,exist_ok=True)
    processes=[]
    with tempfile.TemporaryDirectory(prefix='wb-qt-wayland-') as directory:
        root=Path(directory);sock=root/'test.sock'
        with (ARTIFACTS/'run.log').open('w') as log, (ARTIFACTS/'niri.log').open('w') as niri_log:
            def spawn(args,**kwargs):
                child=subprocess.Popen(args,**kwargs);processes.append(child);return child
            try:
                nested=dict(os.environ,WINIT_UNIX_BACKEND='x11',LIBGL_ALWAYS_SOFTWARE='1')
                for name in ('WAYLAND_DISPLAY','NIRI_SOCKET','QT_QPA_PLATFORM','QT_SCALE_FACTOR','QT_SCREEN_SCALE_FACTORS','QT_FONT_DPI','WORKBENCH_TEST_KEYBOARD_LAYOUT','WORKBENCH_TEST_KEYBOARD_VARIANT'):
                    nested.pop(name,None)
                rd,wr=os.pipe()
                try:
                    spawn(['Xvfb','-displayfd',str(wr),'-screen','0','2560x1600x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log)
                    os.close(wr);wr=None
                    assert select.select([rd],[],[],5)[0], 'Xvfb did not start'
                    with os.fdopen(rd) as pipe:nested['DISPLAY']=':'+pipe.readline().strip()
                    rd=None
                finally:
                    for fd in (rd,wr):
                        if fd is not None:os.close(fd)
                (root/'niri.kdl').write_text('hotkey-overlay { skip-at-startup; }\nanimations { off; }\nprefer-no-csd\n')
                compositor=spawn(['niri','-c',str(root/'niri.kdl')],env=nested,stdout=niri_log,stderr=niri_log)
                def endpoints():
                    assert compositor.poll() is None, 'Nested Niri exited'
                    value=(ARTIFACTS/'niri.log').read_text()
                    display=re.search(r'Wayland socket: (\S+)',value);ipc=re.search(r'IPC listening on: (\S+)',value)
                    return (display[1],ipc[1]) if display and ipc else None
                display,ipc=wait_until(endpoints,timeout=15)
                env=dict(nested,WAYLAND_DISPLAY=display,NIRI_SOCKET=ipc,QT_QPA_PLATFORM='wayland',QT_WAYLAND_DISABLE_WINDOWDECORATION='1')
                env.pop('DISPLAY',None)  # Qt cannot silently fall back to XWayland.
                def run(*args):return subprocess.run(args,env=env,capture_output=True,check=True,timeout=10)
                def xrun(*args):return subprocess.run(['xdotool',*map(str,args)],env=nested,capture_output=True,check=True,timeout=10)
                parent=wait_until(lambda:xrun('search','--onlyvisible','--pid',compositor.pid).stdout.decode().strip()).splitlines()[0]
                xrun('windowsize','--sync',parent,'1920','1200');time.sleep(.3)
                def action(*args):return run('niri','msg','action',*map(str,args))
                def windows():return json.loads(run('niri','msg','-j','windows').stdout)
                protocol=ROOT/'tests/fixtures/virtual-keyboard-unstable-v1.xml'
                run('wayland-scanner','client-header',str(protocol),str(root/'virtual-keyboard.h'))
                run('wayland-scanner','private-code',str(protocol),str(root/'virtual-keyboard.c'))
                flags=run('pkg-config','--cflags','--libs','wayland-client','xkbcommon').stdout.decode().split()
                run('cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),str(ROOT/'tests/wayland_keyboard.c'),str(root/'virtual-keyboard.c'),'-o',str(root/'keyboard'),*flags)
                def key(value,*mods):run(str(root/'keyboard'),'key',value,*mods)
                def text(value):run(str(root/'keyboard'),'text',value)
                (root/'reader.py').write_text(READER)
                spawn([str(runtime),str(sock),'/bin/sh'],cwd=root,stdout=log,stderr=log)
                wait_until(sock.exists);connection,_,_=attach(sock)
                send(connection,16,('python3 '+str(root/'reader.py')+'\n').encode())
                wait_until((root/'reader-ready').exists);connection.close()
                pid=(root/'reader-pid').read_text()
                def received():return (root/'received').read_bytes() if (root/'received').exists() else b''
                def report(path):
                    try:return json.loads(path.read_text())
                    except (OSError,ValueError):return {}
                def launch(name,ime=False):
                    path=ARTIFACTS/(name+'.json');path.unlink(missing_ok=True)
                    args=[str(viewer),'--control',str(sock.with_suffix('.ctl')),'--test-report',str(path)]
                    if ime:args.append('--test-ime')
                    child=spawn(args,cwd=root,env=env,stdout=log,stderr=log)
                    def mapped():
                        assert child.poll() is None, 'Qt viewer exited'
                        return report(path).get('ready') and windows()
                    wait_until(mapped,timeout=15)
                    wait_until(lambda:'QT-SNAPSHOT' in report(path).get('text',''))
                    assert report(path)['platform']=='wayland',report(path)
                    action('move-window-to-floating');action('set-window-width','800');action('set-window-height','500')
                    action('move-floating-window','--x','70','--y','50');time.sleep(.3)
                    return child,path
                child,path=launch('first',True)
                wait_until(lambda:report(path).get('ime_done'))
                wait_until(lambda:received()=='日本'.encode());expected='日本'.encode()
                (root/'live').touch();wait_until(lambda:'LIVE-OUTPUT' in report(path).get('text',''))
                text('Aa9');key('Return');key('c','Control');key('Tab');key('Left')
                expected+=b'Aa9\r\x03\t\x1b[D';wait_until(lambda:received()==expected)
                print('PASS: native Qt Wayland snapshot/live output, commit-only composition and exact keys',flush=True)
                observations=[]
                def clipboard():
                    reply=subprocess.run(['wl-paste','--no-newline','--type','text/plain;charset=utf-8'],env=env,capture_output=True,timeout=5)
                    return reply.stdout if reply.returncode==0 else b''
                def point(data,column,row,scale):
                    layout=windows()[0]['layout'];origin=layout['tile_pos_in_workspace_view'];offset=layout['window_offset_in_tile']
                    x=origin[0]+offset[0]+data['x']+8+(column+.25)*data['cell_width']
                    y=origin[1]+offset[1]+data['y']+8+(row+.5)*data['cell_height']
                    xrun('mousemove','--window',parent,round(x*scale),round(y*scale));time.sleep(.08)
                def copy_range(start,end,row,scale,value):
                    # Force a fresh offer so stale clipboard text cannot make this pass.
                    sentinel='sentinel-'+str(time.monotonic_ns())
                    spawn(['wl-copy','--foreground','--',sentinel],env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=log)
                    wait_until(lambda:clipboard()==sentinel.encode())
                    data=report(path)
                    xrun('mousemove','--window',parent,1910,1190);time.sleep(.08)
                    point(data,start,row,scale);xrun('mousedown',1)
                    point(data,end,row,scale);xrun('mouseup',1);time.sleep(.08)
                    key('c','Control','Shift');wait_until(lambda:clipboard()==value.encode())
                original_grid=wait_until(lambda:(root/'size').read_text())
                original_window=(report(path)['window_width'],report(path)['window_height'])
                for index,scale in enumerate((1,1.25,1.5,2,1)):
                    run('niri','msg','output','winit','scale',str(scale))
                    wait_until(lambda:abs(report(path).get('device_pixel_ratio',0)-scale)<.01,timeout=10)
                    time.sleep(.25)
                    data=report(path)
                    assert (data['window_width'],data['window_height'])==original_window,(scale,data)
                    assert (root/'size').read_text()==original_grid,(scale,original_grid,(root/'size').read_text())
                    assert 'LIVE-OUTPUT' in data['text']
                    copy_range(0,10,0,scale,'copy sample')
                    copy_range(10,0,0,scale,'copy sample')
                    copy_range(0,5,1,scale,'café λ')
                    key('v','Control','Shift');expected+=b'\x1b[200~'+'café λ'.encode()+b'\x1b[201~'
                    text('scale'+str(index));expected+=('scale'+str(index)).encode()
                    wait_until(lambda:received()==expected)
                    capture=Path(str(path)+'.capture');capture.touch()
                    wait_until(lambda:not capture.exists())
                    image=Path(str(path)+'.png');header=image.read_bytes()[:24]
                    assert header[:8]==b'\x89PNG\r\n\x1a\n'
                    pixels=struct.unpack('!II',header[16:24])
                    for logical,actual in zip(original_window,pixels):assert abs(actual-logical*scale)<=1,(scale,original_window,pixels)
                    shutil.copy2(image,ARTIFACTS/f'scale-{index}-{scale}.png')
                    observations.append(dict(scale=scale,pixels=pixels,logical=original_window,grid=original_grid))
                    assert (root/'reader-pid').read_text()==pid
                    print('PASS: native scale, backing pixels, stable grid and forward/reverse/Unicode clipboard',scale,flush=True)
                (ARTIFACTS/'display.json').write_text(json.dumps(observations,indent=2))
                xrun('click',4)
                wait_until(lambda:'history-' in report(path).get('text',''))
                text('live');expected+=b'live'
                wait_until(lambda:'LIVE-OUTPUT' in report(path).get('text',''))
                wait_until(lambda:received()==expected)
                print('PASS: native Wayland scrollback and typing returns to live content',flush=True)
                # Reopen at fractional density, preserving the existing shell/VT.
                run('niri','msg','output','winit','scale','1.5')
                wait_until(lambda:abs(report(path).get('device_pixel_ratio',0)-1.5)<.01)
                action('close-window');child.wait(timeout=10);wait_until(lambda:not windows())
                child,path=launch('reopened')
                wait_until(lambda:abs(report(path).get('device_pixel_ratio',0)-1.5)<.01)
                wait_until(lambda:'LIVE-OUTPUT' in report(path).get('text',''))
                copy_range(0,10,0,1.5,'copy sample');key('v','Control','Shift')
                expected+=b'\x1b[200~copy sample\x1b[201~';text('reopened');expected+=b'reopened'
                wait_until(lambda:received()==expected);assert (root/'reader-pid').read_text()==pid
                before=(root/'size').read_text();action('set-window-width','650')
                def resized():
                    data=report(path)
                    if not data or data.get('window_width',0)>=800:return False
                    expected_grid=(int((data['height']-16)/data['cell_height']),int((data['width']-16)/data['cell_width']))
                    return (root/'size').read_text() not in ('',before) and (root/'size').read_text()==str(expected_grid)
                wait_until(resized)
                os.kill(int(pid),0)
                action('close-window');child.wait(timeout=10)
                print('PASS: fractional startup selection/paste, compositor PTY resize and same-PID reattach; DISPLAY unset',flush=True)
            finally:
                subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=12)
                for process in reversed(processes):stop(process)
                if (root/'received').exists():shutil.copy2(root/'received',ARTIFACTS/'received.bin')
    return 0


if __name__=='__main__':sys.exit(main())
