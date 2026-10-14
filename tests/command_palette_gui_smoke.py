#!/usr/bin/env python3
"""Palette routing with live shells: navigation, dialogs and input isolation."""
from gui_smoke import close_window
from chrome_gui_actions import action as chrome_action
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
    with tempfile.TemporaryDirectory(prefix='wb-commands-') as temporary:
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
                    WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root),WORKBENCH_SHORTCUTS=str(root/'shortcuts.conf'),WORKBENCH_REDUCED_MOTION='1')
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
                    cmd('keydown','--window',window,value);time.sleep(.05)
                    cmd('keyup','--window',window,value);time.sleep(.07)
                def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',15,'--',value)
                def click(x,y):
                    cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.05);cmd('mouseup',1);time.sleep(.08)
                def line(value):text(value);key('Return')
                def marker(name):return wait_until(lambda:(root/name).exists() and (root/name).read_text())
                def catalog():return json.loads((state/'workspaces.json').read_text())
                def palette(query,mouse=False,run=True):
                    if mouse:chrome_action(cmd,window,'Open Commands')
                    else:key('ctrl+shift+p')
                    text(query)
                    if run:key('Return')
                line('keep=palette; printf "%s" "$$" > original.pid');pid=marker('original.pid')
                before=(state/'workspaces.json').read_bytes()
                # Cancelled and no-match input must never reach the shell.
                palette('touch leaked-query',mouse=True,run=False);key('Escape')
                palette('touch leaked-no-match');key('Escape')
                assert (state/'workspaces.json').read_bytes()==before
                line('printf "%s:%s" "$$" "$keep" > isolated');assert marker('isolated')==pid+':palette'
                assert not (root/'leaked-query').exists() and not (root/'leaked-no-match').exists()
                # Dispatch consumes Enter: it cannot immediately save the opened dialog.
                palette('Rename workspace');time.sleep(.2)
                assert (state/'workspaces.json').read_bytes()==before
                key('ctrl+a');text('Palette project');key('Return')
                wait_until(lambda:catalog()['workspaces'][0]['name']=='Palette project');time.sleep(.3)
                line('printf "%s:%s" "$$" "$keep" > renamed');assert marker('renamed')==pid+':palette'
                palette('New terminal');text('Palette shell');key('Return')
                wait_until(lambda:len(catalog()['workspaces'][0]['terminals'])==3);time.sleep(.3)
                line('printf "%s" "$$" > new.pid');newpid=marker('new.pid');assert newpid!=pid
                palette('Previous terminal');line('printf "%s" "$$" > previous.pid');middlepid=marker('previous.pid')
                assert middlepid not in (pid,newpid)
                palette('Next terminal');line('printf "%s" "$$" > next.pid');assert marker('next.pid')==newpid
                # Two matching results exercise arrows and mouse selection.
                palette('Focus the',run=False);key('Down');key('Return')
                # Next is unavailable at the last terminal; the palette stays open.
                key('Up');key('Return')
                line('printf "%s" "$$" > arrows.pid');assert marker('arrows.pid')==middlepid
                palette('Focus the',run=False)
                # Palette bounds at 1240x760: first result is y=235..277.
                click(420,255)
                line('printf "%s" "$$" > clicked.pid');assert marker('clicked.pid')==pid
                palette('Focus the',run=False);cmd('click',5);key('Return')
                line('printf "%s" "$$" > wheel.pid');assert marker('wheel.pid')==middlepid
                palette('Next terminal');line('printf "%s" "$$" > after-wheel.pid');assert marker('after-wheel.pid')==newpid
                palette('Next workspace');line('printf "%s" "$$" > scratch.pid');scratchpid=marker('scratch.pid')
                palette('Previous workspace');line('printf "%s" "$$" > return.pid');assert marker('return.pid')==newpid
                # Stop actions only open the mouse-confirmed stop dialog.
                before=(state/'workspaces.json').read_bytes()
                palette('Stop terminal');key('Return');time.sleep(.2);os.kill(int(newpid),0)
                assert (state/'workspaces.json').read_bytes()==before;key('Escape')
                line('printf stopped-safely > stop-cancel');assert marker('stop-cancel')=='stopped-safely'
                palette('project folder');key('Escape')
                palette('Change appearance');key('Escape')
                palette('Open overview');key('Escape')
                line('printf overlay-safe > overlays');assert marker('overlays')=='overlay-safe'
                # Disabled result stays in palette and cannot start a shell.
                palette('Start new shell');key('Return');key('Escape');os.kill(int(newpid),0)
                # Default navigation routes only in the working view.
                key('ctrl+shift+Left');line('printf "%s" "$$" > nav-left');assert marker('nav-left')==middlepid
                key('ctrl+shift+Right');line('printf "%s" "$$" > nav-right');assert marker('nav-right')==newpid
                key('ctrl+shift+Down');line('printf "%s" "$$" > nav-down');assert marker('nav-down')==scratchpid
                key('ctrl+shift+Up');line('printf "%s" "$$" > nav-up');assert marker('nav-up')==newpid
                # A shortcut opening an editor cannot save it in the same frame.
                before=(state/'workspaces.json').read_bytes();key('ctrl+shift+t');key('Escape')
                assert (state/'workspaces.json').read_bytes()==before
                # Settings captures a chord, rejects collisions, then saves a valid one.
                palette('Shortcut settings');text('Next terminal');key('Return')
                key('ctrl+c');key('Return');time.sleep(.2)
                assert not (root/'shortcuts.conf').exists()
                key('ctrl+shift+p');key('Return');time.sleep(.2)
                assert not (root/'shortcuts.conf').exists()
                key('F12');time.sleep(.2)
                artifacts=ROOT/'.local/test-results';artifacts.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1],artifacts/'shortcuts-conflict.png')
                key('ctrl+shift+j');key('Return');time.sleep(.2)
                key('F12');time.sleep(.2)
                shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1],artifacts/'shortcuts.png')
                key('Escape')
                assert 'terminal.next=Ctrl+Shift+J' in (root/'shortcuts.conf').read_text()
                key('ctrl+shift+Left');key('ctrl+shift+j')
                line('printf "%s" "$$" > custom-nav');assert marker('custom-nav')==newpid
                # A cancelled recording leaves the complete saved file unchanged.
                saved=(root/'shortcuts.conf').read_bytes()
                palette('Shortcut settings');text('Next terminal');key('Return');key('ctrl+shift+k');key('Escape');key('Escape')
                assert (root/'shortcuts.conf').read_bytes()==saved
                # Clear and Default restore an individual action, with no shell restart.
                palette('Shortcut settings');text('Next terminal');key('Return');click(470,452);click(761,452);key('Escape')
                assert 'terminal.next=none' in (root/'shortcuts.conf').read_text()
                palette('Shortcut settings');text('Next terminal');key('Return');click(550,452);click(761,452);key('Escape')
                assert 'terminal.next=Ctrl+Shift+Right' in (root/'shortcuts.conf').read_text()
                # Send-next-key bypasses the application and uses Ghostty's encoder.
                raw_script=root/'raw_keys.py'
                raw_script.write_text("import os,sys,tty,termios,select,time\nfrom pathlib import Path\nsaved=termios.tcgetattr(0)\ntry:\n tty.setraw(0);Path('raw.ready').write_text('ready');assert select.select([0],[],[],8)[0];data=os.read(0,128);time.sleep(.2);Path('raw.bytes').write_text(data.hex())\nfinally:termios.tcsetattr(0,termios.TCSADRAIN,saved)\n")
                line('python3 raw_keys.py');marker('raw.ready')
                palette('Send next key');key('ctrl+shift+Left')
                assert bytes.fromhex(marker('raw.bytes'))==b'\x1b[1;6D'
                line('printf "%s" "$$" > forwarded-focus');assert marker('forwarded-focus')==newpid
                palette('Send next key');key('Escape');key('ctrl+shift+Left')
                line('printf "%s" "$$" > cancelled-forward');assert marker('cancelled-forward')==middlepid
                key('ctrl+shift+Right')
                # Unbinding clipboard shortcuts must also disable the old handler.
                palette('Shortcut settings');text('Paste clipboard');key('Return');click(470,452);click(761,452);key('Escape')
                assert 'paste=none' in (root/'shortcuts.conf').read_text(), (root/'shortcuts.conf').read_text()
                (root/'raw.ready').unlink();(root/'raw.bytes').unlink()
                line('python3 raw_keys.py');marker('raw.ready');key('ctrl+shift+v')
                assert bytes.fromhex(marker('raw.bytes'))==b'\x1b[118;6u'
                (root/'raw.ready').unlink();(root/'raw.bytes').unlink()
                line('python3 raw_keys.py');marker('raw.ready');palette('Send next key');key('ctrl+shift+p')
                assert bytes.fromhex(marker('raw.bytes'))==b'\x1b[112;6u'
                # Navigation bindings have no effect inside an existing modal.
                palette('Rename workspace');key('ctrl+shift+Left');key('Escape')
                line('printf "%s" "$$" > modal-scope');assert marker('modal-scope')==newpid
                # Fill organization externally to exercise live refresh and capacity.
                for i in range(6):request(state,runtime,root,'workspace.create',title='Capacity '+str(i))
                time.sleep(.5);before=(state/'workspaces.json').read_bytes()
                palette('New workspace',run=False);key('F12');time.sleep(.3)
                artifacts=ROOT/'.local/test-results';artifacts.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1],artifacts/'commands-capacity.png')
                key('Return');time.sleep(.2);assert (state/'workspaces.json').read_bytes()==before;key('Escape')
                line('printf capacity-safe > capacity');assert marker('capacity')=='capacity-safe'
                # Minimum window height still has keyboard-accessible results.
                cmd('windowsize',window,800,300);time.sleep(.2)
                palette('Next workspace');line('printf "%s" "$$" > small.pid');assert marker('small.pid')==scratchpid
                # Ordinary terminal control input stays available outside the palette.
                line('sleep 30');key('ctrl+c');line('printf ctrl-c-safe > controls');assert marker('controls')=='ctrl-c-safe'
                # A fresh viewer loads the saved override while shells survive.
                palette('Shortcut settings');text('Next terminal');key('Return');key('ctrl+shift+j');key('Return');key('Escape')
                close_window(window,env);assert process.wait(timeout=5)==0
                process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
                window=wait_until(find);cmd('windowfocus',window);time.sleep(.4)
                key('ctrl+shift+j');line('printf "%s" "$$" > restored-binding');assert marker('restored-binding')==middlepid
                close_window(window,env);assert process.wait(timeout=5)==0
                for value in (pid,newpid,middlepid,scratchpid):os.kill(int(value),0)
                print('PASS: palette mouse/chord entry, filtering/cancel/no-match isolation, dialog handoff, create/rename, same-PID navigation, stop confirmation safety, theme/overview/folder routing, disabled recovery/capacity, default/custom navigation, recorder collision/cancel/clear/default, raw forwarded arrow, unbound clipboard input, modal scope, saved override on reopen, cancelled forwarding, minimum geometry and ordinary Ctrl-C')
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
