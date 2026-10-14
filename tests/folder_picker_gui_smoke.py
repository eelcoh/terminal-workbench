#!/usr/bin/env python3
"""Real Zenity selection/cancel, editable values and unchanged live siblings."""
from pathlib import Path
import json,os,select,subprocess,sys,tempfile,time
from session_smoke import wait_until
from gui_smoke import close_window
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request
runtime,viewer=[Path(value).resolve() for value in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='wb-pick-ui-') as temporary:
    root=Path(temporary).resolve();state=root/'s';chosen=root/'chosen folder';chosen.mkdir();processes=[]
    os.environ['SHELL']='/bin/sh'
    def call(method='initialize',identity='',title=''):return request(state,runtime,root,method,identity,title)
    def data():return json.loads((state/'workspaces.json').read_text())
    with (root/'gui.log').open('w+') as log:
        try:
            call();pid=wait_until(lambda:json.loads((state/'spike.lifecycle').read_text()).get('shell_pid'));saved=(state/'workspaces.json').read_bytes()
            rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as pipe:display=pipe.readline().strip()
            env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',GDK_BACKEND='x11',GTK_USE_PORTAL='0',GSK_RENDERER='cairo',GSETTINGS_BACKEND='memory',XDG_CONFIG_HOME=str(root/'config'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'),
                WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root))
            for key in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CONFIG'):env.pop(key,None)
            bus=subprocess.Popen(['dbus-daemon','--session','--nofork','--print-address=1'],env=env,stdout=subprocess.PIPE,stderr=log,text=True);processes.append(bus)
            env['DBUS_SESSION_BUS_ADDRESS']=bus.stdout.readline().strip()
            wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
            process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
            def command(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            def find():
                assert process.poll() is None
                out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                return out.stdout.splitlines()[-1] if out.returncode==0 else None
            window=wait_until(find);command('windowfocus',window);time.sleep(.5)
            def key(value,target=None):
                target=target or window;command('keydown','--window',target,value);time.sleep(.08);command('keyup',value);time.sleep(.15)
            def text(value):command('type','--clearmodifiers','--window',window,'--delay',15,'--',value)
            def palette(value):key('ctrl+shift+p');text(value);key('Return')
            def picker():
                out=subprocess.run(['xdotool','search','--onlyvisible','--name','^Choose project folder$'],env=env,capture_output=True,text=True)
                return out.stdout.splitlines()[-1] if out.returncode==0 else None
            def choose():
                native=wait_until(picker,timeout=10);command('windowfocus',native);key('ctrl+l',native)
                subprocess.run(['xclip','-selection','clipboard'],env=env,input=str(chosen).encode(),check=True)
                key('ctrl+v',native);key('Return',native)
                if picker():key('Return',native)
                wait_until(lambda:not picker(),timeout=10);command('windowfocus',window);time.sleep(.5)
            palette('Change project folder');key('Tab');key('Return')
            native=wait_until(picker,timeout=10);command('windowfocus',native);key('Escape',native)
            wait_until(lambda:not picker());command('windowfocus',window);time.sleep(.3)
            assert (state/'workspaces.json').read_bytes()==saved
            key('Escape');palette('Change project folder');key('Tab');key('Return');choose()
            assert (state/'workspaces.json').read_bytes()==saved,'picker saved without confirmation'
            key('Return');wait_until(lambda:data()['workspaces'][0].get('directory')==str(chosen))
            assert json.loads((state/'spike.lifecycle').read_text())['shell_pid']==pid
            palette('New workspace');text('Picked project');key('Tab');key('Tab');key('Return');choose()
            assert len(data()['workspaces'])==2
            key('Return');wait_until(lambda:len(data()['workspaces'])==3)
            assert data()['workspaces'][-1]['name']=='Picked project' and data()['workspaces'][-1]['directory']==str(chosen)
            palette('Change project folder');key('Tab');key('Return');wait_until(picker,timeout=10)
            close_window(window,env);assert process.wait(timeout=5)==0
            wait_until(lambda:not picker(),timeout=10)
            assert call('terminal.status','spike')['state']=='running'
            print('PASS: real Linux folder picker, keyboard Browse, Cancel preservation, explicit Save/Create, selected directories, pending-picker cleanup and live sibling identity')
        finally:
            for path in state.glob('*.sock'):subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=15)
            try:call('service.shutdown')
            except (ValueError,OSError):pass
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                process.wait(timeout=5)
            if sys.exc_info()[0]:log.seek(0);print(log.read(),file=sys.stderr)
