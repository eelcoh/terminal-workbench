#!/usr/bin/env python3
"""Palette routing with live shells: navigation, dialogs and input isolation."""
from pathlib import Path
import json
import os
import select
import shutil
import subprocess
import sys
import tempfile
import time
from session_smoke import wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request


def main():
    runtime,viewer=[Path(arg).resolve() for arg in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-help-') as temporary:
        root=Path(temporary);state=root/'sessions';state.mkdir();processes=[]
        with (root/'test.log').open('w+') as log:
            env=dict(os.environ,SHELL='/bin/sh')
            os.environ['SHELL']='/bin/sh'
            try:
                data=request(state,runtime,root)['data']
                rd,wr=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-nolisten','tcp','-noreset'],pass_fds=(wr,),stdout=log,stderr=log)
                processes.append(xvfb);os.close(wr)
                assert select.select([rd],[],[],5)[0]
                # Xvfb writes the number and newline separately. Do not close
                # its pipe after a short read of just the number (EPIPE).
                with os.fdopen(rd) as ready:display=ready.readline().strip()
                assert display.isdigit()
                env.update(DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),
                    WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_STATE_DIR=str(state),
                    WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root),WORKBENCH_SHORTCUTS=str(root/'shortcuts.conf'),WORKBENCH_WINDOW_STATE=str(root/'window.view'),WORKBENCH_REDUCED_MOTION=os.environ.get('WORKBENCH_REDUCED_MOTION','0'))
                env.pop('WAYLAND_DISPLAY',None);env.pop('WORKBENCH_SOCKET',None)
                # Xvfb may publish its display number before XKB setup completes.
                wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
                process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
                def cmd(*args):
                    result=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5)
                    result.check_returncode();return result.stdout.strip()
                def find():
                    assert process.poll() is None
                    result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                    return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
                window=wait_until(find);cmd('windowfocus',window);time.sleep(.4)
                def key(value):
                    cmd('keydown','--window',window,value);time.sleep(.12 if value=='F12' else .06)
                    cmd('keyup','--window',window,value);time.sleep(.07)
                def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',15,'--',value)
                def click(x,y):
                    cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.05);cmd('mouseup',1);time.sleep(.08)
                def line(value):text(value);key('Return')
                def marker(name):return wait_until(lambda:(root/name).exists() and (root/name).read_text())
                def catalog():return json.loads((state/'workspaces.json').read_text())
                def palette(query,mouse=False,run=True):
                    if mouse:click(80,745)
                    else:key('ctrl+shift+p')
                    text(query)
                    if run:key('Return')
                line('keep=help; printf "%s" "$$" > original.pid');pid=marker('original.pid')
                catalog_before=(state/'workspaces.json').read_bytes()
                import fcntl,termios
                def geometry():
                    fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                    try:return fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                    finally:os.close(fd)
                baseline=geometry()
                artifacts=ROOT/'.local/test-results';artifacts.mkdir(parents=True,exist_ok=True)
                def capture(name):
                    started=time.time_ns();key('F12')
                    image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=started),None))
                    destination=artifacts/name;shutil.copyfile(image,destination);return destination
                def safe(name):
                    line('printf "%s:%s" "$$" "$keep" > '+name);assert marker(name)==pid+':help'
                    assert geometry()==baseline
                # '?' text from Shift+/ must be consumed along with the help chord.
                key('ctrl+shift+slash');capture('shortcut-help-wide.png')
                text('touch help-leak');key('ctrl+shift+t');key('ctrl+shift+Right');key('ctrl+shift+Down')
                key('Down');key('Page_Down');key('Home');key('End');key('ctrl+shift+p');key('ctrl+shift+o')
                assert geometry()==baseline and (state/'workspaces.json').read_bytes()==catalog_before
                key('ctrl+shift+slash');safe('help-toggle-safe');assert not (root/'help-leak').exists()
                palette('Show shortcuts',mouse=True);key('Escape');safe('help-palette-safe')
                # Help is layered over Overview without losing its search or selection.
                key('ctrl+shift+o');text('Main');key('ctrl+shift+slash');text('touch overview-leak')
                key('ctrl+shift+o');key('Escape');key('Return');safe('help-overview-safe')
                assert not (root/'overview-leak').exists()
                # Shortcut recorder accepts slash and saves a customized map.
                palette('Shortcut settings');text('Next terminal');key('Return');key('ctrl+alt+slash');key('Return');key('Escape')
                assert 'terminal.next=Ctrl+Alt+/' in (root/'shortcuts.conf').read_text()
                palette('Shortcut settings');text('Show shortcuts');key('Return');key('ctrl+shift+h');key('Return');key('Escape')
                assert 'shortcuts.help=Ctrl+Shift+H' in (root/'shortcuts.conf').read_text()
                palette('Shortcut settings');text('New workspace');key('Return');click(470,452);click(761,452);key('Escape')
                assert 'workspace.new=none' in (root/'shortcuts.conf').read_text()
                key('ctrl+shift+h');capture('shortcut-help-custom.png')
                # Enter leads to settings and does not also select/save an action.
                key('Return');text('Show shortcuts');key('Return');key('Escape');key('Escape');safe('help-settings-safe')
                # Remapped slash navigation routes only outside the help overlay.
                key('ctrl+alt+slash');line('printf "%s" "$$" > second.pid');other_pid=marker('second.pid');assert other_pid!=pid
                key('ctrl+shift+Left');safe('slash-nav-return')
                # Dialogs retain input ownership even if help's chord is pressed.
                palette('Rename workspace');key('ctrl+shift+h');key('Escape');safe('help-dialog-safe')
                # Minimum height uses a single scrollable column; headers/footer remain reachable.
                key('ctrl+shift+h');cmd('windowsize',window,800,300);time.sleep(.3)
                key('End');small_end=capture('shortcut-help-small.png')
                cmd('mousemove','--window',window,650,150);cmd('click',4);key('Home')
                small_home=capture('shortcut-help-small-home.png')
                from theme_gui_smoke import pixel
                def body_signature(path):return tuple(pixel(path,x,y) for y in (124,130,153,159) for x in range(58,350,7))
                assert body_signature(small_end)!=body_signature(small_home), 'cached help did not redraw after scrolling'
                assert geometry()==baseline, 'help window resize changed PTY geometry'
                click(712,56);time.sleep(.3)
                # Closing help applies ordinary working-view geometry at the resized window.
                baseline=geometry();safe('help-small-close')
                cmd('windowsize',window,1240,760);time.sleep(.3);baseline=geometry()
                # Light appearance uses the existing theme surface tokens.
                palette('Change appearance');text('Dawn');key('Return')
                key('ctrl+shift+h');light=capture('shortcut-help-light.png')
                from theme_gui_smoke import pixel
                assert pixel(light,160,64)[0]>200, 'help did not use the light application theme'
                key('Escape')
                os.kill(int(pid),0);os.kill(int(other_pid),0)
                from gui_smoke import close_window
                close_window(window,env);assert process.wait(timeout=5)==0
                print('PASS: help slash/toggle/Commands entry, grouped live/custom/unbound bindings, overview layering, modal isolation, settings/recorder handoff, slash navigation, narrow scrolling and same PID/state/geometry')
            finally:
                if sys.exc_info()[0]:
                    for sock in state.glob('*.sock'):
                        screen=subprocess.run([str(runtime),'read',str(sock)],capture_output=True,text=True,timeout=5)
                        print(sock.name,screen.stdout[-1800:],file=sys.stderr)
                for process in reversed(processes):
                    if process.poll() is None:process.terminate();process.wait(timeout=5)
                for sock in state.glob('*.sock'):subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=15)
                request(state,runtime,root,'service.shutdown')
                if sys.exc_info()[0]:log.seek(0);print(log.read(),file=sys.stderr)
if __name__=='__main__':main()
