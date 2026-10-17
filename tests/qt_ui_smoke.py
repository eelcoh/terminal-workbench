"""Real native input and PTY invariants for the isolated Qt UI slice (Linux)."""
import json
import os
import re
from pathlib import Path
import select
import shlex
import subprocess
import sys
import tempfile
import time
from session_smoke import attach,send,wait_until
from gui_smoke import close_window

runtime,viewer=[str(Path(value).resolve()) for value in sys.argv[1:3]]
wayland='--wayland' in sys.argv[3:]
ROOT=Path(__file__).resolve().parent.parent
artifacts=ROOT/'.local/artifacts/qt-ui'/('wayland' if wayland else 'xcb');artifacts.mkdir(parents=True,exist_ok=True)
report=artifacts/'report.json';report.unlink(missing_ok=True)
processes=[];sockets=[]
with tempfile.TemporaryDirectory(prefix='wb-ui-test-',dir='/tmp') as directory, (artifacts/'run.log').open('w') as log:
    root=Path(directory)
    try:
        reader=root/'reader.py';reader.write_text('''import os,tty,time,select,termios,struct,fcntl
from pathlib import Path
tty.setraw(0);Path('pid').write_text(str(os.getpid()));os.write(1,b'UI-READY')
while True:
 if select.select([0],[],[],.02)[0]:
  data=os.read(0,65536)
  with Path('input').open('ab') as output:output.write(data)
 rows,cols,_,_=struct.unpack('HHHH',fcntl.ioctl(0,termios.TIOCGWINSZ,b'\\0'*8));Path('size').write_text(str(cols)+','+str(rows))
 if Path('update').exists():os.write(1,b'\\r\\nHIDDEN-UPDATE');Path('update').unlink()
''')
        for i in range(6):
            actor=root/str(i);actor.mkdir();sock=actor/'p.sock';sockets.append(sock)
            server=subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=actor,stdout=log,stderr=log);processes.append(server)
            wait_until(sock.exists);c,_,_=attach(sock)
            send(c,16,(shlex.join(['exec',sys.executable,str(reader)])+'\n').encode());c.close()
            wait_until((actor/'pid').exists)
        pids=[(root/str(i)/'pid').read_text() for i in range(6)]
        config=root/'sessions.json';config.write_text(json.dumps([str(sock.with_suffix('.ctl')) for sock in sockets]))
        rd,wr=os.pipe();x=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1500x1100x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(x);os.close(wr)
        assert select.select([rd],[],[],5)[0]
        with os.fdopen(rd) as stream:display=stream.readline().strip()
        env=dict(os.environ,DISPLAY=':'+display,QT_QPA_PLATFORM='xcb',LIBGL_ALWAYS_SOFTWARE='1');env.pop('WAYLAND_DISPLAY',None)
        def cmd(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,check=True,capture_output=True,text=True).stdout.strip()
        nested=dict(env)
        if wayland:
            (root/'niri.kdl').write_text('hotkey-overlay { skip-at-startup; }\nanimations { off; }\nprefer-no-csd\n')
            niri_log=(artifacts/'niri.log').open('w')
            nested['WINIT_UNIX_BACKEND']='x11'
            compositor=subprocess.Popen(['niri','-c',str(root/'niri.kdl')],env=nested,stdout=niri_log,stderr=niri_log);processes.append(compositor)
            def endpoints():
                assert compositor.poll() is None,'private Niri exited'
                value=(artifacts/'niri.log').read_text();display=re.search(r'Wayland socket: (\S+)',value);ipc=re.search(r'IPC listening on: (\S+)',value)
                return (display[1],ipc[1]) if display and ipc else None
            display,ipc=wait_until(endpoints,timeout=15)
            env.update(WAYLAND_DISPLAY=display,NIRI_SOCKET=ipc,QT_QPA_PLATFORM='wayland',QT_WAYLAND_DISABLE_WINDOWDECORATION='1');env.pop('DISPLAY',None)
            def run(*args):return subprocess.run(list(map(str,args)),env=env,check=True,capture_output=True,text=True,timeout=10).stdout
            def host(*args):return subprocess.run(['xdotool',*map(str,args)],env=nested,check=True,capture_output=True,text=True,timeout=10).stdout.strip()
            parent=wait_until(lambda:host('search','--onlyvisible','--pid',compositor.pid)).splitlines()[0];host('windowsize','--sync',parent,1500,1100)
            protocol=ROOT/'tests/fixtures/virtual-keyboard-unstable-v1.xml'
            run('wayland-scanner','client-header',protocol,root/'virtual-keyboard.h');run('wayland-scanner','private-code',protocol,root/'virtual-keyboard.c')
            flags=run('pkg-config','--cflags','--libs','wayland-client','xkbcommon').split()
            run('cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),ROOT/'tests/wayland_keyboard.c',root/'virtual-keyboard.c','-o',root/'keyboard',*flags)
        client=subprocess.Popen([viewer,str(config),'--test-report',str(report)],env=env,stdout=log,stderr=log);processes.append(client)
        def data():
            try:return json.loads(report.read_text())
            except (FileNotFoundError,ValueError):return {}
        wait_until(lambda:len(data().get('panes',[]))==6 and all(p['ready'] and 'UI-READY' in p['text'] for p in data()['panes']),timeout=30)
        if wayland:
            assert data()['platform']=='wayland' and 'DISPLAY' not in env
            window=wait_until(lambda:next((w['id'] for w in json.loads(run('niri','msg','-j','windows')) if w['title']=='Terminal Workbench · Qt UI study'),None))
            run('niri','msg','action','focus-window','--id',window)
            run('niri','msg','action','move-window-to-floating');run('niri','msg','action','set-window-width','1240');run('niri','msg','action','set-window-height','760')
            run('niri','msg','action','move-floating-window','--x','70','--y','50');time.sleep(.35)
        else:
            window=cmd('search','--sync','--name','^Terminal Workbench · Qt UI study$').splitlines()[0];cmd('windowfocus',window)
        def key(value):
            if wayland:
                pieces=value.split('+');mods=[{'super':'Mod4','alt':'Mod1'}.get(v,v) for v in pieces[:-1]]
                run(root/'keyboard','key',pieces[-1],*mods)
            else:cmd('key','--window',window,value)
        def typed(value):
            if wayland:run(root/'keyboard','text',value)
            else:cmd('type','--window',window,'--delay',1,value)
        def click(x,y,button=1):
            if wayland:
                layout=next(w['layout'] for w in json.loads(run('niri','msg','-j','windows')) if w['id']==window)
                origin=layout['tile_pos_in_workspace_view'];offset=layout['window_offset_in_tile']
                host('mousemove','--window',parent,round(origin[0]+offset[0]+x),round(origin[1]+offset[1]+y));host('click',button)
            else:cmd('mousemove','--window',window,int(x),int(y));cmd('click',button)
        def received(i):
            path=root/str(i)/'input';return path.read_bytes() if path.exists() else b''
        def sizes():return [(p['columns'],p['rows']) for p in data()['panes']]
        def capture(name):
            Path(str(report)+'.capture').touch();wait_until(lambda:not Path(str(report)+'.capture').exists());(artifacts/name).write_bytes(Path(str(report)+'.png').read_bytes())
        wait_until(lambda:data()['panes'][0]['focus']);typed('first');wait_until(lambda:received(0)==b'first')
        initial_sizes=sizes();pane=data()['panes'][0];click(pane['x'],pane['y'],7)
        wait_until(lambda:data()['content_x']>0)
        assert sizes()==initial_sizes and data()['panes'][0]['focus'],'horizontal wheel changed grid or focus'
        key('super+alt+Left');wait_until(lambda:data()['content_x']<1)
        key('super+alt+Right');wait_until(lambda:data()['selected']==1 and data()['panes'][1]['focus']);typed('second');wait_until(lambda:received(1)==b'second')
        offscreen_rows=data()['panes'][2]['rasterized_rows']
        (root/'2'/'update').touch();wait_until(lambda:'HIDDEN-UPDATE' in data()['panes'][2]['text'])
        assert data()['panes'][2]['rasterized_rows']==offscreen_rows,'offscreen column rasterized output'
        original=sizes();key('super+alt+r');wait_until(lambda:sizes()[1][0]>original[1][0]);wide=sizes()
        assert all(wide[i]==original[i] for i in (0,2,3,4,5)),'width changed a sibling'
        key('super+alt+f');wait_until(lambda:sizes()[1][0]>wide[1][0]);key('super+alt+f');wait_until(lambda:sizes()==wide)
        key('super+alt+Down');wait_until(lambda:data()['workspace']==1 and data()['panes'][3]['focus']);typed('peer');wait_until(lambda:received(3)==b'peer')
        hidden_rows=data()['panes'][0]['rasterized_rows']
        (root/'0'/'update').touch();wait_until(lambda:'HIDDEN-UPDATE' in data()['panes'][0]['text'])
        assert data()['panes'][0]['rasterized_rows']==hidden_rows,'background workspace rasterized output'
        before=sizes();key('super+alt+o');wait_until(lambda:data()['overview']);time.sleep(.35)
        assert sizes()==before,'Overview changed PTY geometry';capture('overview.png')
        pane=data()['panes'][4];click(pane['x'],pane['y'])
        wait_until(lambda:not data()['overview'] and data()['selected']==4 and data()['panes'][4]['focus']);typed('chosen');wait_until(lambda:received(4)==b'chosen')
        key('super+alt+o');wait_until(lambda:data()['overview']);key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][4]['focus'])
        key('Escape');wait_until(lambda:received(4)==b'chosen\x1b')
        key('super+alt+slash');time.sleep(.15);key('Escape');wait_until(lambda:data()['panes'][4]['focus'])
        key('super+alt+Down');wait_until(lambda:data()['workspace']==2 and data()['selected']==-1)
        key('super+alt+Up');wait_until(lambda:data()['workspace']==1 and data()['selected']==4 and data()['panes'][4]['focus'])
        capture('working.png')
        expected=[b'first',b'second',b'',b'peer',b'chosen\x1b',b'']
        assert [received(i) for i in range(6)]==expected,'navigation/overview shortcuts leaked into a PTY'
        for i,pid in enumerate(pids):assert (root/str(i)/'pid').read_text()==pid;os.kill(int(pid),0)
        if wayland:run('niri','msg','action','close-window','--id',window)
        else:close_window(window,env)
        client.wait(timeout=5);assert client.returncode==0
        print(('wayland' if wayland else 'xcb')+' PASS: native shortcuts, per-pane width/full restore, workspace focus, live hidden output, Overview selection/Escape, unchanged grids/PIDs and isolated input')
    finally:
        for sock in sockets:subprocess.run([runtime,'stop',str(sock)],capture_output=True,timeout=12)
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
