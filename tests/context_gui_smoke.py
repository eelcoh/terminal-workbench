#!/usr/bin/env python3
"""Overview context searches route keyboard focus to the correct live shell."""
from pathlib import Path
import fcntl
import json
import os
import select
import shlex
import shutil
import subprocess
import sys
import tempfile
import termios
import time
from gui_smoke import close_window
from session_smoke import attach, send, wait_until

runtime,viewer_binary=[str(Path(arg).resolve()) for arg in sys.argv[1:3]]
ROOT=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-context-ui-') as directory:
    root=Path(directory);state=root/'sessions';state.mkdir()
    repo=root/'repository';repo.mkdir();(repo/'src').mkdir()
    subprocess.run(['git','-C',str(repo),'init','-b','context-main'],check=True,capture_output=True)
    processes=[];pids={}
    environment=dict(os.environ,SHELL='/bin/sh',LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),WORKBENCH_WINDOW_STATE=str(root/'window.view'))
    environment.pop('WAYLAND_DISPLAY',None);environment.pop('WORKBENCH_SOCKET',None)
    with (root/'gui.log').open('w+') as log:
        try:
            # Old catalog format remains supported and generic titles gain context.
            (state/'workspaces.tsv').write_text('workbench-catalog-v3\nW\tw\tProjects\nT\tw\tfirst\tShell 1\t'+str(state/'first.sock')+'\nT\tw\tsecond\tShell 2\t'+str(state/'second.sock')+'\n')
            for identity,cwd in [('first',repo),('second',root)]:
                process=subprocess.Popen([runtime,str(state/(identity+'.sock')),'/bin/sh'],cwd=cwd,stderr=log);processes.append(process)
                wait_until((state/(identity+'.sock')).exists)
                connection,_,_=attach(state/(identity+'.sock'))
                send(connection,16,('echo $$ > '+shlex.quote(str(root/(identity+'.pid')))+'\n').encode())
                wait_until((root/(identity+'.pid')).exists);pids[identity]=int((root/(identity+'.pid')).read_text());connection.close()
            read_fd,write_fd=os.pipe()
            xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(write_fd,),stdout=log,stderr=log);processes.append(xvfb);os.close(write_fd)
            assert select.select([read_fd],[],[],5)[0]
            with os.fdopen(read_fd) as ready: display=ready.readline().strip()
            assert display.isdigit(), 'Xvfb did not finish writing its display number'
            environment['DISPLAY']=':'+display
            viewer=subprocess.Popen([viewer_binary],env=environment,cwd=root,stdout=log,stderr=log);processes.append(viewer)
            def command(*args):
                return subprocess.run(['xdotool',*map(str,args)],env=environment,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            def find():
                assert viewer.poll() is None
                result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(viewer.pid)],env=environment,capture_output=True,text=True)
                return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
            window=wait_until(find,timeout=10);command('windowfocus',window);time.sleep(1.5)
            def key(name):
                command('keydown','--window',window,name);time.sleep(.06);command('keyup','--window',window,name);time.sleep(.3)
            def text(value):command('type','--clearmodifiers','--window',window,'--delay',10,'--',value)
            def verify(identity,name):
                text('echo $$ > '+shlex.quote(str(root/name)));key('Return')
                value=wait_until(lambda:(root/name).exists() and (root/name).read_text())
                assert int(value)==pids[identity],(identity,value)
            def geometry():
                result=[]
                for pid in pids.values():
                    fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NONBLOCK)
                    try:result.append(fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8)))
                    finally:os.close(fd)
                return result
            def search(query):
                key('ctrl+shift+o');time.sleep(.3);before=geometry();text(query);time.sleep(.2)
                assert geometry()==before
                key('Return');time.sleep(.4)
            # Start on the other shell. A branch-only search must select the repo.
            key('ctrl+shift+Right');verify('second','before-context')
            search('context-main');verify('first','branch-selected')
            text('cd src');key('Return');time.sleep(1.4)
            subprocess.run(['git','-C',str(repo),'checkout','-b','changed-context'],check=True,capture_output=True);time.sleep(1.4)
            key('ctrl+shift+Right');verify('second','before-branch-update')
            search('changed-context');verify('first','updated-branch-selected')
            key('ctrl+shift+Right');search(str(repo/'src'));verify('first','cwd-selected')
            # Explicit generic custom names remain visible, while branch/cwd are searchable.
            (state/'terminal-labels.tsv').write_text('workbench-terminal-labels-v1\nfirst\t1\nsecond\t0\n')
            projection=(state/'workspaces.tsv').read_text();(state/'next.tsv').write_text(projection);(state/'next.tsv').replace(state/'workspaces.tsv');time.sleep(.8)
            key('ctrl+shift+Right');search('changed-context');verify('first','custom-branch-selected')
            key('ctrl+shift+o');time.sleep(.5)
            # Capture both layouts for manual visual review outside the user debug folder.
            artifacts=ROOT/'.local/artifacts/context';artifacts.mkdir(parents=True,exist_ok=True)
            key('F12');time.sleep(.2);shutil.copy(sorted(root.glob('screenshot*.png'))[-1],artifacts/'spatial.png')
            command('mousemove','--window',window,840,74);command('click',1);time.sleep(.3)
            key('F12');time.sleep(.2);shutil.copy(sorted(root.glob('screenshot*.png'))[-1],artifacts/'list.png')
            key('Escape');key('ctrl+u') # Screenshot shortcut can also reach the shell.
            close_window(window,environment);viewer.wait(timeout=5)
            assert viewer.returncode==0
            for pid in pids.values():os.kill(pid,0)
            print('PASS: live branch/cwd search, correct shell focus, custom-name projection, spatial/list context, unchanged Overview grids')
        except Exception:
            log.flush();print((root/'gui.log').read_text()[-4000:]);raise
        finally:
            for identity in pids:subprocess.run([runtime,'stop',str(state/(identity+'.sock'))],capture_output=True,timeout=10)
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
