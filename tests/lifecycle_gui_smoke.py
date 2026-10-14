#!/usr/bin/env python3
"""Stopped-pane reason, confirmed recovery and sibling/process continuity."""
from pathlib import Path
import fcntl,json,os,select,shutil,subprocess,sys,tempfile,termios,time
from gui_smoke import close_window
from session_smoke import wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request
runtime,viewer=[Path(x).resolve() for x in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='wb-lifecycle-ui-') as directory:
    root=Path(directory);state=root/'sessions';processes=[];os.environ['SHELL']='/bin/sh'
    def call(method='initialize',identity=''):return request(state,runtime,root,method,identity)
    def record(identity='spike'):return json.loads((state/(identity+'.lifecycle')).read_text())
    with (root/'test.log').open('w+') as log:
        try:
            call();old_pid=wait_until(lambda:record().get('shell_pid'));sibling=record('main-2')['shell_pid']
            rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as pipe:display=pipe.readline().strip()
            env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root),WORKBENCH_REDUCED_MOTION='1')
            for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CONFIG'):env.pop(name,None)
            wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
            process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
            def cmd(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            def find():
                assert process.poll() is None
                out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                return out.stdout.strip().splitlines()[-1] if out.returncode==0 else None
            window=wait_until(find);cmd('windowfocus',window);time.sleep(.4)
            def key(value):cmd('keydown','--window',window,value);time.sleep(.07);cmd('keyup','--window',window,value);time.sleep(.1)
            def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',12,'--',value)
            def line(value):text(value);key('Return')
            def palette(value):key('ctrl+shift+p');text(value);key('Return')
            def capture(name):
                start=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=start),None))
                target=ROOT/'.local/artifacts/lifecycle'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target)
            before=(state/'workspaces.json').read_bytes()
            line('exit 42');wait_until(lambda:'Shell exited with code 42.' in (root/'test.log').read_text());capture('exit-42.png')
            assert record()['exit_code']==42 and record()['shell_pid']==old_pid
            palette('Start new shell');capture('confirmation.png');key('Return');time.sleep(.15)
            assert not (state/'spike.sock').exists(),'Opening/Enter silently restarted the shell'
            key('Escape');assert record()['exit_code']==42 and (state/'workspaces.json').read_bytes()==before
            palette('Start new shell');key('Tab');key('Tab');key('Return')
            wait_until(lambda:record()['event']=='running' and record()['shell_pid']!=old_pid);time.sleep(.4)
            line('printf "%s" "$$" > new.pid');new_pid=wait_until(lambda:(root/'new.pid').exists() and (root/'new.pid').read_text())
            assert int(new_pid)==record()['shell_pid'] and record()['exit_code'] is None
            assert record('main-2')['shell_pid']==sibling and (state/'workspaces.json').read_bytes()==before
            # Runtime loss has an honest unrecorded reason and the same recovery guard.
            os.kill(record()['runtime_pid'],9);wait_until(lambda:'Runtime disappeared; no shell exit was recorded.' in (root/'test.log').read_text());capture('runtime-disappeared.png')
            assert call('terminal.lifecycle','spike')['lifecycle']['reason']=='missing_exit'
            close_window(window,env);assert process.wait(timeout=5)==0
            assert call('terminal.status','main-2')['state']=='running','Closing viewer stopped a sibling'
            print('PASS: exit-code and unrecorded-runtime feedback, retained reason in confirmation, Cancel/Enter guards, explicit fresh recovery and sibling identity')
        finally:
            if (state/'workspaces.json').exists():
                for w in json.loads((state/'workspaces.json').read_text())['workspaces']:
                    for t in w['terminals']:subprocess.run([str(runtime),'stop',str(state/(t['id']+'.sock'))],capture_output=True,timeout=12)
            try:call('service.shutdown')
            except (OSError,ValueError):pass
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                process.wait(timeout=5)
            if sys.exc_info()[0]:log.seek(0);print(log.read(),file=sys.stderr)
