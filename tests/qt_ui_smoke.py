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
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request
import workspace_catalog
os.environ['SHELL']='/bin/sh'
processes=[];sockets=[]
with tempfile.TemporaryDirectory(prefix='wb-ui-test-',dir='/tmp') as directory, (artifacts/'run.log').open('w') as log:
    root=Path(directory);project=root/'Project folder';project.mkdir()
    try:
        reader=root/'reader.py';reader.write_text('''import os,tty,time,select,termios,struct,fcntl
from pathlib import Path
tty.setraw(0);Path('pid').write_text(str(os.getpid()));os.write(1,(b'\\x1b]4;2;rgb:11/22/33\\x07' if Path.cwd().name=='3' else b'')+b'\\x1b[2J\\x1b[HUI-READY\\r\\n\\x1b[31mR\\x1b[0m\\x1b[38;2;1;2;3mT\\x1b[0m\\x1b[32mG\\x1b[0m')
while True:
 if select.select([0],[],[],.02)[0]:
  data=os.read(0,65536)
  with Path('input').open('ab') as output:output.write(data)
 rows,cols,_,_=struct.unpack('HHHH',fcntl.ioctl(0,termios.TIOCGWINSZ,b'\\0'*8));Path('size').write_text(str(cols)+','+str(rows))
 if Path('cursor').exists():os.write(1,Path('cursor').read_bytes());Path('cursor').unlink()
 if Path('update').exists():os.write(1,b'\\r\\nHIDDEN-UPDATE');Path('update').unlink()
''')
        for i in range(6):
            actor=root/str(i);actor.mkdir();sock=root/f'terminal-{i}.sock';sockets.append(sock)
            server=subprocess.Popen([runtime,str(sock),'/bin/sh'],cwd=actor,stdout=log,stderr=log);processes.append(server)
            wait_until(sock.exists);c,_,_=attach(sock)
            try:
                send(c,16,(shlex.join(['exec',sys.executable,str(reader)])+'\n').encode())
                wait_until((actor/'pid').exists)
            finally:c.close()
        pids=[(root/str(i)/'pid').read_text() for i in range(6)]
        catalog={'version':1,'workspaces':[{'id':f'workspace-{w}','name':['Workbench','Side project','Empty workspace'][w],
                 'directory':str(root),'terminals':[{'id':f'terminal-{i}','name':'Shell'} for i in range(w*3,min(w*3+3,6))]} for w in range(3)]}
        workspace_catalog.save(root,catalog)
        catalog=request(root,Path(runtime),root)['data']
        config=root/'sessions.json';config.write_text(json.dumps({'state':str(root),'runtime':runtime,'cwd':str(root),'catalog':catalog}))
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
        def ready_ui():
            assert client.poll() is None,('Qt UI exited during startup',client.returncode,artifacts/'run.log')
            return len(data().get('panes',[]))==6 and all(p['ready'] and 'UI-READY' in p['text'] for p in data()['panes'])
        wait_until(ready_ui,timeout=30)
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
                pieces=value.split('+');mods=[{'super':'Mod4','alt':'Mod1','ctrl':'Control','shift':'Shift'}.get(v,v) for v in pieces[:-1]]
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
        # Focus/style changes must paint real cursor pixels without replacing readers.
        wait_until(lambda:data()['panes'][0]['cursor']=='block' and data()['panes'][1]['cursor']=='outline')
        capture('cursor-active.png');pixels=json.loads(Path(str(report)+'.cursor.json').read_text())
        assert pixels[0]['cursor_pixels']>pixels[0]['area']*.75,pixels[0]
        assert pixels[1]['cursor_pixels']<pixels[1]['area']*.5 and pixels[1]['background_pixels']>0,pixels[1]
        cursor_grid=sizes()
        def cursor(sequence,style):
            (root/'0'/'cursor').write_bytes(sequence)
            wait_until(lambda:data()['panes'][0]['cursor']==style)
        cursor(b'\x1b[2;1H\x1b[2 q','block')
        wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.1)
        capture('cursor-character.png');pixels=json.loads(Path(str(report)+'.cursor.json').read_text())
        assert pixels[0]['cursor_pixels']>0 and pixels[0]['background_pixels']>0,'filled cursor hid the character underneath'
        cursor(b'\x1b[6 q','bar');capture('cursor-bar.png')
        cursor(b'\x1b[4 q','underline');capture('cursor-underline.png')
        cursor(b'\x1b[?25l','hidden');capture('cursor-hidden.png');pixels=json.loads(Path(str(report)+'.cursor.json').read_text())
        assert pixels[0]['cursor_pixels']==0,'hidden cursor still painted'
        cursor(b'\x1b[?25h\x1b[2 q','block')
        key('super+alt+slash');wait_until(lambda:data()['panes'][0]['cursor']=='outline')
        key('Escape');wait_until(lambda:data()['panes'][0]['cursor']=='block')
        assert sizes()==cursor_grid
        initial_sizes=sizes();pane=data()['panes'][0];click(pane['x'],pane['y'],7)
        wait_until(lambda:data()['content_x']>0)
        assert sizes()==initial_sizes and data()['panes'][0]['focus'],'horizontal wheel changed grid or focus'
        key('super+alt+Left');wait_until(lambda:data()['content_x']<1)
        key('super+alt+Right');wait_until(lambda:data()['selected']==1 and data()['panes'][1]['focus'] and data()['panes'][1]['cursor']=='block' and data()['panes'][0]['cursor']=='outline');typed('second');wait_until(lambda:received(1)==b'second')
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
        # Preview changes existing indexed/default colors, preserves explicit RGB and PTYs.
        theme_catalog_before=(root/'workspaces.json').read_bytes();theme_sizes=sizes()
        def appearance():
            click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
            click(*data()['appearance_menu_item']);wait_until(lambda:data()['appearance_open'] and data()['theme_search_focus'])
        def theme_state(index,background,red):
            value=data()
            return value['panes'][3]['sample_green']=='#112233' and value['chrome_background']==background and all(p['theme']==index and p['background']==background and p['sample_red']==red and p['sample_rgb']=='#010203' for p in value['panes'][3:6])
        appearance();typed('Workbench Dawn');wait_until(lambda:data()['theme_matches']==1);key('Down')
        wait_until(lambda:theme_state(1,'#f4f1eb','#a93f4a'))
        assert all(p['theme']==0 and p['background']=='#15191f' for p in data()['panes'][:3])
        assert sizes()==theme_sizes;capture('theme-light-preview.png')
        key('Escape');wait_until(lambda:not data()['appearance_open'] and data()['panes'][4]['focus'] and theme_state(0,'#15191f','#e49b9b'))
        appearance();typed('Dracula');wait_until(lambda:data()['theme_matches']==1);key('Down')
        wait_until(lambda:theme_state(2,'#282a36','#ff5555'));key('Return')
        wait_until(lambda:not data()['appearance_open'] and data()['panes'][4]['focus'])
        assert theme_state(2,'#282a36','#ff5555') and sizes()==theme_sizes
        assert (root/'workspaces.json').read_bytes()==theme_catalog_before,'appearance mutated runtime organization'
        appearance();typed('no-theme-matches-this');wait_until(lambda:data()['theme_matches']==0);capture('theme-no-results.png');key('Escape')
        wait_until(lambda:not data()['appearance_open']);assert theme_state(2,'#282a36','#ff5555')
        key('super+alt+Up');wait_until(lambda:data()['workspace']==0 and data()['chrome_background']=='#15191f')
        key('super+alt+Down');wait_until(lambda:data()['workspace']==1 and data()['chrome_background']=='#282a36')
        # Creation uses the real service, keeping the original six attached readers.
        key('super+alt+t')
        wait_until(lambda:len(data()['panes'])==7 and data()['panes'][6]['ready'] and data()['selected']==6 and data()['panes'][6]['focus'])
        wait_until(lambda:data()['panes'][6]['theme']==2 and data()['panes'][6]['background']=='#282a36')
        created=workspace_catalog.load(root)['workspaces'][1]['terminals'][-1]['id']
        marker=root/'new-terminal-cwd'
        typed('pwd > '+shlex.quote(str(marker)));key('Return');wait_until(marker.exists)
        assert marker.read_text().strip()==str(root)
        key('super+alt+n');wait_until(lambda:data()['creation_open'])
        typed('Project test');key('Tab');key('ctrl+a');typed(str(root/'missing'));key('Return')
        wait_until(lambda:'Folder' in data()['error'] and not data()['busy'])
        assert data()['workspace_count']==3 and len(data()['panes'])==7,'failed creation changed catalog'
        capture('invalid-folder.png')
        # Failed Create keeps fields editable; replace the invalid folder in place.
        key('Escape');wait_until(lambda:not data()['creation_open'])
        click(24,28);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['new_workspace_button']);wait_until(lambda:data()['creation_open']);time.sleep(.3)
        assert data()['name_focus'],'closing the drawer stole dialog focus'
        typed('Project test');key('Tab');key('ctrl+a');typed(str(project));key('Return')
        wait_until(lambda:data()['workspace_count']==4 and len(data()['panes'])==8 and not data()['creation_open'] and data()['panes'][7]['ready'] and data()['panes'][7]['focus'])
        assert data()['workspace']==3
        marker2=root/'workspace-cwd';typed('pwd > '+shlex.quote(str(marker2)));key('Return');wait_until(marker2.exists)
        assert marker2.read_text().strip()==str(project)
        created_workspace=workspace_catalog.load(root)['workspaces'][-1]
        assert created_workspace['name']=='Project test' and created_workspace['directory']==str(project)
        # Empty workspace can create its first terminal without special handling.
        key('super+alt+Up');wait_until(lambda:data()['workspace']==2 and data()['selected']==-1)
        key('super+alt+t');wait_until(lambda:len(data()['panes'])==9 and data()['panes'][8]['ready'] and data()['panes'][8]['focus'])
        wait_until(lambda:all(p['geometry_ready'] for p in data()['panes']))
        creation_sizes=sizes();key('super+alt+o');wait_until(lambda:data()['overview']);time.sleep(.3)
        assert sizes()==creation_sizes,('Overview resized dynamically created terminals',creation_sizes,sizes())
        capture('created-overview.png');key('Escape');wait_until(lambda:not data()['overview'])
        capture('working.png')
        expected=[b'first',b'second',b'',b'peer',b'chosen\x1b',b'']
        assert [received(i) for i in range(6)]==expected,'navigation/overview shortcuts leaked into a PTY'
        for i,pid in enumerate(pids):assert (root/str(i)/'pid').read_text()==pid;os.kill(int(pid),0)
        if wayland:run('niri','msg','action','close-window','--id',window)
        else:close_window(window,env)
        client.wait(timeout=5);assert client.returncode==0
        print(('wayland' if wayland else 'xcb')+' PASS: native shortcuts, per-pane width/full restore, workspace focus, live hidden output, Overview selection/Escape, focus-aware cursor pixels/app shapes/hidden/glyph preservation, theme preview/cancel/apply/isolation/RGB+OSC preservation, creation/invalid-folder/cancel/empty-workspace, unchanged reader PIDs and isolated input')
    finally:
        for sock in root.glob('*.sock'):subprocess.run([runtime,'stop',str(sock)],capture_output=True,timeout=12)
        request(root,Path(runtime),root,"service.shutdown")
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
