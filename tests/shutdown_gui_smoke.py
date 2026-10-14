#!/usr/bin/env python3
"""Two real windows, guarded shutdown, frozen targets and explicit restart."""
from pathlib import Path
import json,os,select,shutil,subprocess,sys,tempfile,time
from gui_smoke import close_window
from session_smoke import wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request
runtime,viewer=[Path(x).resolve() for x in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='wb-shutdown-ui-') as directory:
    root=Path(directory).resolve();state=root/'s';processes=[];os.environ['SHELL']='/bin/sh'
    def call(method='initialize',identity='',title='',targets=None):
        return request(state,runtime,root,method,identity,title,terminal_ids=targets)
    def record(identity='spike'):return json.loads((state/(identity+'.lifecycle')).read_text())
    with (root/'test.log').open('w+') as log:
        try:
            original=call()['data'];selected=[t['id'] for w in original['workspaces'] for t in w['terminals']]
            wait_until(lambda:all(record(i).get('shell_pid') for i in selected))
            old_pids={i:record(i)['shell_pid'] for i in selected}
            rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as pipe:display=pipe.readline().strip()
            env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root),WORKBENCH_REDUCED_MOTION='1')
            for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CONFIG'):env.pop(name,None)
            wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
            def cmd(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            def launch():
                process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
                def find():
                    assert process.poll() is None
                    out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                    return out.stdout.strip().splitlines()[-1] if out.returncode==0 else None
                window=wait_until(find);cmd('windowfocus',window);time.sleep(.5)
                return process,window
            first,one=launch();second,two=launch();window=one
            def focus(value):
                global window
                window=value;cmd('windowraise',window);cmd('windowfocus',window);time.sleep(.3)
            def key(value):cmd('keydown','--window',window,value);time.sleep(.07);cmd('keyup','--window',window,value);time.sleep(.1)
            def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',12,'--',value)
            def palette(value):key('ctrl+shift+p');text(value);key('Return')
            def capture(name):
                start=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=start),None))
                target=ROOT/'.local/artifacts/shutdown'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target)
            focus(one);palette('Stop all terminals');key('Return');capture('confirmation.png')
            assert all(record(i)['event']=='running' and record(i)['shell_pid']==old_pids[i] for i in selected)
            key('Tab');key('Return');time.sleep(.3)  # Explicit Cancel.
            assert all(record(i)['event']=='running' for i in selected)
            palette('Stop all terminals')
            # A second caller creates a terminal after confirmation captured its IDs.
            later=call('terminal.create','workspace-0','Later')['data']
            excluded=later['workspaces'][0]['terminals'][-1]['id']
            excluded_pid=wait_until(lambda:record(excluded).get('shell_pid'))
            saved=(state/'workspaces.json').read_bytes()
            key('Tab');key('Tab');key('Return')
            wait_until(lambda:all(record(i)['event']=='stopped' for i in selected))
            wait_until(lambda:'Stopped by Workbench.' in (root/'test.log').read_text())
            assert record(excluded)['event']=='running' and record(excluded)['shell_pid']==excluded_pid
            assert (state/'workspaces.json').read_bytes()==saved
            capture('stopped.png')
            # Other-window disconnected panes recheck status on catalog publication.
            focus(two);time.sleep(.8);capture('other-window.png')
            palette('Start new shell');key('Return');time.sleep(.2)
            assert record()['event']=='stopped','Other-window Enter restarted without selecting confirmation'
            key('Tab');key('Tab');key('Return')
            wait_until(lambda:record()['event']=='running' and record()['shell_pid']!=old_pids['spike'])
            assert (state/'workspaces.json').read_bytes()==saved
            close_window(two,env);assert second.wait(timeout=5)==0
            close_window(one,env);assert first.wait(timeout=5)==0
            assert call('terminal.status','spike')['state']=='running'
            assert call('terminal.status',excluded)['state']=='running'
            assert all(call('terminal.status',i)['state']=='stopped' for i in selected if i!='spike')
            assert call()['data']==later
            print('PASS: two-window shutdown, Cancel/Enter guards, frozen confirmation IDs, later-terminal exclusion, retained catalog, other-window stopped feedback, explicit recovery and close-detach behavior')
        finally:
            for path in state.glob('*.sock'):subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=15)
            try:call('service.shutdown')
            except (OSError,ValueError):pass
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                process.wait(timeout=5)
            if sys.exc_info()[0]:log.seek(0);print(log.read(),file=sys.stderr)
