#!/usr/bin/env python3
"""Keyboard focus with disposable live shells, overlays and explicit confirmations."""
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
    with tempfile.TemporaryDirectory(prefix='wb-focus-') as temporary:
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
                    if mouse:click(22,20);click(90,225)
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
                import struct
                # Existing 7x16 monospace grid at the fixed 1240x760 half-pane layout.
                assert struct.unpack('HHHH',baseline)[:2]==(41,82), 'compact rail changed the expected logical terminal grid'
                if env.get('WORKBENCH_GEOMETRY_OUTPUT'): Path(env['WORKBENCH_GEOMETRY_OUTPUT']).write_text(json.dumps(struct.unpack('HHHH',baseline)[:2]))
                artifacts=ROOT/'.local/test-results';artifacts.mkdir(parents=True,exist_ok=True)
                def capture(name):
                    started=time.time_ns();key('F12')
                    image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=started),None))
                    destination=artifacts/(('retina-' if env.get('WORKBENCH_RENDER_SCALE')=='2' else '')+name);shutil.copyfile(image,destination);return destination
                def safe(name):
                    line('printf "%s:%s" "$$" "$keep" > '+name);assert marker(name)==pid+':help'
                    assert geometry()==baseline
                # Commands: reverse traversal reaches Done; Space closes without executing a result.
                palette('Rename workspace',run=False);key('shift+Tab');key('space');safe('focus-command-cancel')
                assert (state/'workspaces.json').read_bytes()==catalog_before
                # Creation visits both fields and both buttons, skips absent folder field in rename.
                key('ctrl+shift+n');text('Focus project');key('Tab');text(str(root))
                key('Tab');capture('focus-create-cancel.png');key('space')
                assert (state/'workspaces.json').read_bytes()==catalog_before
                palette('Rename workspace');key('Tab');key('space')
                assert (state/'workspaces.json').read_bytes()==catalog_before
                # Explicit button activation creates a workspace with the chosen directory.
                key('ctrl+shift+n');text('Focus project');key('Tab');text(str(root));key('Tab');key('Tab');key('space')
                wait_until(lambda:len(catalog()['workspaces'])==3)
                assert catalog()['workspaces'][-1]['name']=='Focus project'
                assert catalog()['workspaces'][-1]['directory']==str(root)
                key('ctrl+shift+Up');key('ctrl+shift+Up');time.sleep(.2)
                # Help hands off to settings; recording only occurs while its capture field has focus.
                key('ctrl+shift+slash');key('Tab');capture('focus-help-edit.png');key('space')
                text('Next terminal');key('Return');key('Tab');key('space') # Clear.
                key('Tab');key('space') # Default.
                key('Tab');capture('focus-recorder-cancel.png');key('space');key('Escape')
                assert not (root/'shortcuts.conf').exists()
                key('ctrl+shift+slash');key('shift+Tab');key('Return');safe('focus-help-done')
                # Overview filter buttons retain focus and query; help restores that exact focus.
                key('ctrl+shift+o');key('Tab');key('Tab');key('Tab');key('Tab');key('Tab');key('space') # Agents.
                key('ctrl+shift+slash');key('Escape');key('space') # All terminals again.
                key('Tab');key('space');safe('focus-overview-done')
                # Appearance supports scope, follow and cancel without accidental Apply.
                palette('Change appearance');key('Tab');key('Tab');key('Tab');key('space') # Workspace scope.
                for _ in range(3):key('Tab')
                key('space') # Follow toggle.
                capture('focus-appearance-follow.png');key('Escape')
                assert not (state/'appearance.state').exists()
                palette('Change appearance');text('Dawn');key('shift+Tab');key('space') # Apply.
                wait_until(lambda:(state/'appearance.state').exists())
                assert 'background = #f4f1eb' in (state/'appearance.state').read_text()
                palette('Change appearance');key('shift+Tab');key('shift+Tab');capture('focus-appearance-cancel-light.png');key('space')
                safe('focus-appearance-safe')
                # Initial Enter/Space never stops a live shell; an explicit focused confirmation does.
                palette('Stop terminal');key('Return');key('space');os.kill(int(pid),0)
                key('Tab');capture('focus-stop-cancel.png');key('Return');safe('focus-stop-safe')
                # Use a newly created terminal so this test can verify the complete stop/recover transition.
                key('ctrl+shift+t');text('Focus disposable');key('Tab');key('Tab');key('Return')
                wait_until(lambda:len(catalog()['workspaces'][0]['terminals'])==3);time.sleep(.3)
                line('printf "%s" "$$" > disposable.pid');disposable=marker('disposable.pid')
                palette('Stop terminal');key('Return');key('Tab');key('Tab');capture('focus-stop-confirm.png');key('space')
                wait_until(lambda:len(catalog()['workspaces'][0]['terminals'])==2);os.kill(int(pid),0)
                # Recover a shell that exited without deleting its catalog identity.
                key('ctrl+shift+t');text('Focus recovery');key('Return')
                wait_until(lambda:len(catalog()['workspaces'][0]['terminals'])==3);time.sleep(.3)
                recovery_id=catalog()['workspaces'][0]['terminals'][-1]['id'];recovery_path=state/(recovery_id+'.sock')
                line('exit');wait_until(lambda:not recovery_path.exists());time.sleep(.7)
                palette('Start new shell');key('Return');key('space');assert not recovery_path.exists()
                key('Tab');key('Return');assert not recovery_path.exists()
                palette('Start new shell');key('Tab');key('Tab');capture('focus-recovery-confirm.png');key('Return')
                wait_until(recovery_path.exists);time.sleep(.4)
                line('printf recovered > focus-recovered');assert marker('focus-recovered')=='recovered'
                palette('Stop terminal');key('Tab');key('Tab');key('Return')
                wait_until(lambda:len(catalog()['workspaces'][0]['terminals'])==2);time.sleep(.3)
                palette('Previous terminal');time.sleep(.3)
                # Narrow help keeps traversal and Done reachable.
                key('ctrl+shift+slash');cmd('windowsize',window,800,300);time.sleep(.2)
                key('Tab');key('Tab');capture('focus-help-small-done.png');key('space')
                cmd('windowsize',window,1240,760);time.sleep(.3);baseline=geometry();safe('focus-narrow-safe')
                assert not (root/'shortcuts.conf').exists()
                from gui_smoke import close_window
                close_window(window,env);assert process.wait(timeout=5)==0
                log.flush();render_log=(root/'test.log').read_text()
                assert 'Could not load interface font' not in render_log
                if env.get('WORKBENCH_RENDER_SCALE')=='2':
                    assert 'Texture loaded successfully (1166x1400' in render_log, 'pane canvas remained at logical resolution'
                    assert 'Texture loaded successfully (1840x1156' in render_log, 'help cache remained at logical resolution'
                print('PASS: forward/reverse focus traversal, text/button isolation, create/folder/save/cancel, help/settings/recorder, overview focus restoration, appearance scope/follow/apply/cancel, deliberate stop/recovery, narrow focus and live shell continuity')
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
