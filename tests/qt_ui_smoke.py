"""Real native input and PTY invariants for the isolated Qt UI slice (Linux)."""
import json
import ctypes
import os
import re
from pathlib import Path
import select
import shutil
import uuid
import shlex
import subprocess
import sys
import tempfile
import time
from session_smoke import attach,send,wait_until
from gui_smoke import close_window
from snapshot_attach_smoke import snapshot_attach,collect,revoked

runtime,viewer=[str(Path(value).resolve()) for value in sys.argv[1:3]]
wayland='--wayland' in sys.argv[3:]
ROOT=Path(__file__).resolve().parent.parent
artifacts=ROOT/'.local/artifacts/qt-ui'/('wayland' if wayland else 'xcb');artifacts.mkdir(parents=True,exist_ok=True)
report=artifacts/'report.json';report.unlink(missing_ok=True)
(artifacts/'chrome-menu.png').unlink(missing_ok=True)
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request
import workspace_catalog
os.environ['SHELL']='/bin/sh'
processes=[];sockets=[];client=None;xdisplay=None
with tempfile.TemporaryDirectory(prefix='wb-ui-test-',dir='/tmp') as directory, (artifacts/'run.log').open('w') as log:
    root=Path(directory);project=root/'Project folder';project.mkdir();child=project/'child';child.mkdir();chosen=project/'café #100% &';chosen.mkdir()
    try:
        theme_store=root/'theme-cache'
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
        managed=root/'config.conf';managed.write_text('workbench-config-v1\n# managed fixture\n[appearance]\napplication-theme = missing-application-theme\nterminal-theme = Workbench Night\n[macos.font]\nsize = 20\n[font]\nfamily = Fira Code\nsize = 14\n[linux.font]\nsize = 16\n[shortcuts]\ncommands = F10\nshortcuts.help = F9\nworkspace.rename = F8\nforward = F6\n');managed_bytes=managed.read_bytes()
        config=root/'sessions.json';config.write_text(json.dumps({'state':str(root),'view_state':str(root/'qt-view.json'),'catalog_updates':True,'runtime':runtime,'cwd':str(root),'catalog':catalog,'theme_store':str(theme_store),'preferences':str(managed),'shortcut_overrides':str(root/'ui/linux/shortcuts.conf'),'font_overrides':str(root/'ui/linux/terminal-font.json'),'theme_directory':str(root/'themes')}))
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
        display_report=artifacts/'display-report.json';display_report.unlink(missing_ok=True)
        env['WORKBENCH_DISPLAY_REPORT']=str(display_report)
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
        def display_data():
            try:return json.loads(display_report.read_text())
            except (FileNotFoundError,ValueError):return {}
        wait_until(lambda:len(display_data().get('panes',[]))==6 and all(p['ready'] for p in display_data()['panes']))
        display=display_data()
        assert display['schema']==1 and display['platform']==data()['platform'] and display['window_dpr']>0
        assert display['graphics_api']!='other' and display['screen']['logical_dpi']>0
        assert all(p['resolved_family'] and p['cell_width']>0 and p['cell_height']>0 for p in display['panes'])
        assert any(p['presented'] and abs(p['row_image_dpr']-display['window_dpr'])<.01 for p in display['panes'])
        assert 'UI-READY' not in display_report.read_text() and str(root) not in display_report.read_text(),'display diagnostics exposed terminal content or runtime paths'
        # Xdotool incorrectly synthesizes Alt for bare function keys in this XKB map.
        # Use XTest's physical keycode path for those keys, as a real keyboard does.
        xdisplay=None
        if not wayland:
            xlib=ctypes.CDLL('libX11.so.6');xtest=ctypes.CDLL('libXtst.so.6')
            xlib.XOpenDisplay.argtypes=[ctypes.c_char_p];xlib.XOpenDisplay.restype=ctypes.c_void_p
            xlib.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];xlib.XKeysymToKeycode.restype=ctypes.c_uint
            xlib.XFlush.argtypes=[ctypes.c_void_p];xlib.XCloseDisplay.argtypes=[ctypes.c_void_p]
            xtest.XTestFakeKeyEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
            xdisplay=xlib.XOpenDisplay(env['DISPLAY'].encode());assert xdisplay
        def raw_key(value):
            if wayland:
                pieces=value.split('+');mods=[{'super':'Mod4','alt':'Mod1','ctrl':'Control','shift':'Shift'}.get(v,v) for v in pieces[:-1]]
                run(root/'keyboard','key',pieces[-1],*mods)
            elif re.fullmatch(r'F(?:[1-9]|1[01])',value):
                code=xlib.XKeysymToKeycode(xdisplay,0xffbe+int(value[1:])-1);assert code
                assert xtest.XTestFakeKeyEvent(xdisplay,code,1,0) and xtest.XTestFakeKeyEvent(xdisplay,code,0,0)
                xlib.XFlush(xdisplay)
            else:cmd('key','--window',window,value)
        def key(value):
            raw_key({'super+alt+p':'F10','super+alt+slash':'F9'}.get(value,value))
        def typed(value):
            if wayland:run(root/'keyboard','text',value)
            else:cmd('type','--window',window,'--delay',1,value)
        def move(x,y):
            if wayland:
                layout=next(w['layout'] for w in json.loads(run('niri','msg','-j','windows')) if w['id']==window)
                origin=layout['tile_pos_in_workspace_view'];offset=layout['window_offset_in_tile']
                host('mousemove','--window',parent,round(origin[0]+offset[0]+x),round(origin[1]+offset[1]+y))
            else:cmd('mousemove','--window',window,int(x),int(y))
        def button(action):
            (host if wayland else cmd)(action,1)
        def settled_point(read):
            # State notifications can precede Qt Quick's next layout polish.
            # Require the same target in two distinct reports before clicking
            # beside labels whose widths just changed (Copy/Checking status).
            previous=None
            def stable():
                nonlocal previous
                point=read(data());stamp=report.stat().st_mtime_ns
                current=(stamp,point)
                ready=point and previous and stamp!=previous[0] and point==previous[1]
                previous=current
                return point if ready else None
            return wait_until(stable)
        def click(x,y,button=1):
            move(x,y);(host if wayland else cmd)('click',button)
        def show_preview(slot):
            deadline=time.monotonic()+6
            while True:
                pane=data()['panes'][slot]
                if pane['bottom']>175 and pane['top']<620:return
                assert time.monotonic()<deadline,'preview did not scroll into view'
                click(1120,650,5 if pane['top']>=620 else 4);time.sleep(.1)
        def drag(slot,x,y,cancel=False):
            show_preview(slot)
            pane=data()['panes'][slot];start=(pane['x'],(max(60,pane['top'])+min(748,pane['bottom']))/2)
            move(*start);button('mousedown')
            try:
                for i in range(1,9):
                    move(start[0]+(x-start[0])*i/8,start[1]+(y-start[1])*i/8);time.sleep(.025)
                wait_until(lambda:data()['drag_slot']==slot)
                if not cancel and slot == 1 and y < 650: capture('drag-target.png')
                if cancel:
                    key('Escape');wait_until(lambda:data()['drag_slot']==-1)
            finally:button('mouseup')
        def drag_workspace(slot,workspace,allowed=True):
            show_preview(slot)
            pane=data()['panes'][slot]
            start=(pane['x'],(max(105,pane['top'])+min(748,pane['bottom']))/2)
            move(*start);button('mousedown')
            try:
                move(start[0]+30,start[1]);wait_until(lambda:data()['drag_slot']==slot)
                deadline=time.monotonic()+8
                while not 115 < data()['overview_groups'][workspace]+25 < 690:
                    assert time.monotonic()<deadline,'edge scroll did not reach workspace'
                    move(160,735 if data()['overview_groups'][workspace]+25>=690 else 106)
                    time.sleep(.08)
                move(160,data()['overview_groups'][workspace]+25)
                wait_until(lambda:data()['drop_space']==workspace and data()['drop_allowed']==allowed)
                if not allowed:capture('drag-full-workspace.png')
            finally:button('mouseup')
        def workspace_form(mode):
            click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
            click(*data()['rename_workspace_item' if mode=='rename' else 'workspace_folder_item'])
            wait_until(lambda:data()['creation_open'] and data()['workspace_form_mode']==mode and data()['name_focus' if mode=='rename' else 'folder_focus'])
        def browse_folder(expected):
            click(*data()['browse_folder_button'])
            wait_until(lambda:data()['folder_browser_open'] and data()['folder_browser_location']==str(expected))
        def choose_folder(name):
            wait_until(lambda:len(data()['folder_list'])==2)
            for _ in range(3):click(*data()['folder_list'],5)
            wait_until(lambda:not data()['folder_list_moving'])
            point=wait_until(lambda:next((choice['point'] for choice in data()['folder_choices'] if choice['name']==name),None))
            click(*point)
            wait_until(lambda:len(data()['folder_accept'])==2)
            click(*data()['folder_accept']);wait_until(lambda:not data()['folder_browser_open'] and data()['folder_focus'] and not data()['folder_accept'])
        def received(i):
            path=root/str(i)/'input';return path.read_bytes() if path.exists() else b''
        def sizes():return [(p['columns'],p['rows']) for p in data()['panes']]
        def capture(name):
            Path(str(report)+'.capture').touch();wait_until(lambda:not Path(str(report)+'.capture').exists());(artifacts/name).write_bytes(Path(str(report)+'.png').read_bytes())
        assert 'Adwaita' in data()['interface_font'],'bundled interface font was not applied'
        wait_until(lambda:all(p['facts'].get('cwd')==str(root/str(i)) for i,p in enumerate(data()['panes'])))
        wait_until(lambda:data()['panes'][0]['focus'])
        assert not data()['shortcut_error'] and data()['shortcut_sequences']['commands']=='F10' and data()['shortcut_labels']['help']=='F9'
        raw_key('super+alt+p');wait_until(lambda:len(received(0))>0);time.sleep(.1)
        assert not data()['commands_open'],'old managed Commands binding still intercepted'
        (root/'0'/'input').write_bytes(b'')
        raw_key('F8');wait_until(lambda:data()['creation_open'] and data()['workspace_form_mode']=='rename')
        key('Escape');wait_until(lambda:not data()['creation_open'] and data()['panes'][0]['focus'])
        # Titles, blank header space and the outer frame select the actual reader.
        chrome_input=[received(i) for i in range(6)];chrome_sizes=sizes()
        for target,frame in ((1,False),(0,True),(1,False),(0,False)):
            pane=data()['panes'][target]
            click(pane['left']+(2 if frame else 24),pane['y'] if frame else pane['top']+18)
            wait_until(lambda:data()['selected']==target and data()['panes'][target]['focus'])
            time.sleep(.3)
        assert [received(i) for i in range(6)]==chrome_input and sizes()==chrome_sizes,'chrome click changed PTY input or grids'
        typed('first');wait_until(lambda:received(0)==b'first')
        # About is modal and client-only: copying diagnostics never types into PTYs.
        about_input=[received(i) for i in range(6)];about_sizes=sizes()
        click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
        click(*data()['about_menu']);wait_until(lambda:data()['about_open'] and not data()['application_menu_open'])
        click(*data()['about_copy']);wait_until(lambda:data()['about_clipboard_matches'])
        click(*settled_point(lambda d:d['about_details']));wait_until(lambda:data()['about_details_ready']);time.sleep(.2);capture('about-details.png')
        raw_key('super+alt+Right');time.sleep(.1);assert data()['selected']==0,'About leaked navigation shortcut'
        key('Escape');wait_until(lambda:not data()['about_open'] and data()['panes'][0]['focus'])
        key('super+alt+p');wait_until(lambda:data()['commands_open']);typed('About Terminal Workbench')
        wait_until(lambda:len(data()['command_matches'])==1 and data()['command_matches'][0]['id']=='about');key('Return');wait_until(lambda:data()['about_open'])
        click(*data()['about_close']);wait_until(lambda:not data()['about_open'] and data()['panes'][0]['focus'])
        assert [received(i) for i in range(6)]==about_input and sizes()==about_sizes,'About altered input or grids'
        if '--about-only' in sys.argv[3:]:
            assert 'Qt UI: qrc:' not in (artifacts/'run.log').read_text(),'QML warning in About regression'
            print(('wayland' if wayland else 'xcb')+' PASS: About menu/palette, visible details, exact diagnostic copy, modal navigation isolation and restored terminal focus/input/grids')
            sys.exit(0)
        # The configuration panel is modal; managed reload is transactional and
        # defers external updates until a font/shortcut editor closes.
        config_inputs=[received(i) for i in range(6)];config_instances=[p['instance'] for p in data()['panes']];config_sizes=sizes()
        click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
        click(*data()['configuration_menu']);wait_until(lambda:data()['configuration_open'])
        capture('configuration.png');raw_key('super+alt+Right');time.sleep(.1);assert data()['selected']==0
        click(*data()['configuration_reload']);wait_until(lambda:data()['configuration_status']=='Preferences reloaded')
        key('Escape');wait_until(lambda:not data()['configuration_open'] and data()['panes'][0]['focus'])
        changed_config=managed_bytes.replace(b'commands = F10',b'commands = F11').replace(b'[linux.font]\nsize = 16',b'[linux.font]\nsize = 18')
        managed.write_bytes(changed_config)
        wait_until(lambda:data()['font_preferences']['size']==18 and data()['shortcut_sequences']['commands']=='F11')
        raw_key('F11');wait_until(lambda:data()['commands_open']);typed('Reload configuration')
        wait_until(lambda:len(data()['command_matches'])==1 and data()['command_matches'][0]['id']=='configuration-reload')
        key('Return');wait_until(lambda:not data()['commands_open'] and data()['panes'][0]['focus'])
        appearance_config=changed_config.replace(b'terminal-theme = Workbench Night',b'terminal-theme = Workbench Dawn')
        managed.write_bytes(appearance_config)
        wait_until(lambda:data()['chrome_background']=='#f4f1eb' and all(p['theme']==1 for p in data()['panes']))
        managed.write_bytes(changed_config.replace(b'terminal-theme = Workbench Night',b'terminal-theme = nonexistent.default').replace(b'size = 18',b'size = 21'))
        wait_until(lambda:bool(data()['configuration_error']))
        assert data()['chrome_background']=='#f4f1eb' and data()['font_preferences']['size']==18,'missing theme partially applied configuration'
        managed.write_text('broken configuration\n')
        wait_until(lambda:bool(data()['configuration_error']))
        assert data()['font_preferences']['size']==18 and data()['shortcut_sequences']['commands']=='F11','invalid reload discarded last valid preferences'
        click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open']);click(*data()['font_menu_item']);wait_until(lambda:data()['font_settings_open'])
        click(*data()['font_plus']);wait_until(lambda:data()['font_preferences']['size']==19)
        managed.write_bytes(managed_bytes);time.sleep(.4)
        assert data()['font_preferences']['size']==19,'external update discarded an open font draft'
        key('Escape');wait_until(lambda:not data()['font_settings_open'] and data()['font_preferences']['size']==16 and data()['shortcut_sequences']['commands']=='F10' and not data()['configuration_error'] and sizes()==config_sizes)
        assert [received(i) for i in range(6)]==config_inputs and [p['instance'] for p in data()['panes']]==config_instances,'reload changed readers or typed into PTYs'
        assert managed.read_bytes()==managed_bytes and not (root/'ui/linux/terminal-font.json').exists()
        if '--configuration-only' in sys.argv[3:]:
            assert 'Qt UI: qrc:' not in (artifacts/'run.log').read_text(),'QML warning in configuration regression'
            print(('wayland' if wayland else 'xcb')+' PASS: configuration panel, manual/automatic reload, live managed font/chord changes, last-valid rejection, editor deferral, restored input/focus/grids and retained readers')
            sys.exit(0)
        # Deliver real window wheel events, including fractional motion and sideways jitter.
        wheel_instances=[p['instance'] for p in data()['panes']];wheel_sizes=sizes()
        def wheel(pixel,angle,count=1,inverted=False):
            path=Path(str(report)+'.wheel')
            temporary=path.with_suffix('.wheel.tmp');temporary.write_text(json.dumps({'pane':0,'pixel':pixel,'angle':angle,'count':count,'inverted':inverted}));temporary.replace(path)
            wait_until(lambda:not path.exists());time.sleep(.15)
        (root/'0'/'cursor').write_bytes(b''.join(('SCROLL-HISTORY-%03d\r\n'%i).encode() for i in range(100))+b'\x1b[2J\x1b[HUI-READY\r\n\x1b[31mR\x1b[0m\x1b[38;2;1;2;3mT\x1b[0m\x1b[32mG\x1b[0m')
        wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.2)
        strip_x=data()['content_x']
        wheel([1,2],[1,2],100)
        wait_until(lambda:'SCROLL-HISTORY-' in data()['panes'][0]['text'])
        assert abs(data()['content_x']-strip_x)<1,'vertical jitter moved horizontal strip'
        wheel([-1,-2],[-1,-2],100)
        wait_until(lambda:'UI-READY' in data()['panes'][0]['text'] and 'SCROLL-HISTORY-' not in data()['panes'][0]['text'])
        wheel([0,0],[0,120],inverted=True)
        wait_until(lambda:'SCROLL-HISTORY-' in data()['panes'][0]['text'])
        wheel([0,0],[0,-120],inverted=True)
        wait_until(lambda:'UI-READY' in data()['panes'][0]['text'] and 'SCROLL-HISTORY-' not in data()['panes'][0]['text'])
        assert received(0)==b'first' and sizes()==wheel_sizes and [p['instance'] for p in data()['panes']]==wheel_instances,'scrolling changed PTY input, grids or readers'
        (root/'0'/'cursor').write_bytes(b'\x1b[3J\x1b[2J\x1b[HUI-READY\r\n\x1b[31mR\x1b[0m\x1b[38;2;1;2;3mT\x1b[0m\x1b[32mG\x1b[0m')
        wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.2)
        # Find scans retained output, owns typing, wraps results and restores the viewport.
        search_input=[received(i) for i in range(6)];search_sizes=sizes();search_instances=[p['instance'] for p in data()['panes']]
        (root/'0'/'cursor').write_bytes(b''.join((('QT-FIND-NEEDLE' if i in (20,40,60) else 'find-history-%03d'%i)+'\r\n').encode() for i in range(120))+b'\x1b[2J\x1b[HUI-READY\r\n\x1b[31mR\x1b[0m\x1b[38;2;1;2;3mT\x1b[0m\x1b[32mG\x1b[0m')
        wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.2)
        search_start=data()['panes'][0]['text']
        wheel([-120,0],[-120,0],8);wait_until(lambda:data()['content_x']>0)
        raw_key('super+alt+s');wait_until(lambda:data()['terminal_search_open'] and data()['terminal_search_focus'])
        typed('QT-FIND-NEEDLE');wait_until(lambda:data()['terminal_search_state'].get('count')==3 and not data()['terminal_search_state']['busy'])
        wait_until(lambda:abs(data()['content_x'])<1)
        assert data()['terminal_search_state']['selected']==3 and data()['terminal_search_state']['highlight_spans']>0
        capture('terminal-search.png')
        key('Return');wait_until(lambda:data()['terminal_search_state']['selected']==1)
        key('shift+Return');wait_until(lambda:data()['terminal_search_state']['selected']==3)
        key('Tab');key('Return');wait_until(lambda:data()['terminal_search_state']['selected']==2)
        key('Tab');key('Return');wait_until(lambda:data()['terminal_search_state']['selected']==3)
        key('shift+Tab');key('space');wait_until(lambda:data()['terminal_search_state']['selected']==2)
        click(*data()['terminal_search_next']);wait_until(lambda:data()['terminal_search_state']['selected']==3)
        key('Escape');wait_until(lambda:not data()['terminal_search_open'] and data()['panes'][0]['focus'] and data()['panes'][0]['text']==search_start)
        raw_key('super+alt+s');wait_until(lambda:data()['terminal_search_focus'])
        typed('not-present-here');wait_until(lambda:not data()['terminal_search_state']['busy'] and data()['terminal_search_state']['count']==0)
        raw_key('super+alt+t');raw_key('super+alt+Right');time.sleep(.1)
        assert len(data()['panes'])==6 and data()['selected']==0 and not data()['overview']
        key('ctrl+a');typed('QT-FIND-NEEDLE');wait_until(lambda:data()['terminal_search_state']['count']==3)
        (root/'0'/'cursor').write_bytes(b'\r\nQT-FIND-NEEDLE')
        wait_until(lambda:data()['terminal_search_state']['count']==4 and not data()['terminal_search_state']['busy'])
        click(*data()['terminal_search_close']);wait_until(lambda:not data()['terminal_search_open'] and data()['panes'][0]['focus'])
        assert [received(i) for i in range(6)]==search_input and sizes()==search_sizes and [p['instance'] for p in data()['panes']]==search_instances,'search changed shell input, grids or reader identity'
        (root/'0'/'cursor').write_bytes(b'\x1b[3J\x1b[2J\x1b[HUI-READY\r\n\x1b[31mR\x1b[0m\x1b[38;2;1;2;3mT\x1b[0m\x1b[32mG\x1b[0m')
        wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.2)
        if '--search-only' in sys.argv[3:]:
            if wayland:run('niri','msg','action','close-window','--id',window)
            else:close_window(window,env)
            client.wait(timeout=5);assert client.returncode==0
            assert 'Qt UI: qrc:' not in (artifacts/'run.log').read_text(),'QML warning in search regression'
            print(('wayland' if wayland else 'xcb')+' PASS: native search, offscreen reveal, input isolation, wrap, keyboard/mouse match controls, output refresh and viewport restoration')
            sys.exit(0)
        # Pinning reserves layout space; navigation stays usable without modal focus.
        nav_sizes=sizes();nav_instances=[p['instance'] for p in data()['panes']]
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['navigation_pin']);wait_until(lambda:data()['navigation_docked'] and not data()['drawer_open'] and all(p['geometry_ready'] for p in data()['panes']))
        capture('navigation-pinned.png')
        click(*data()['navigation_workspace_1']);wait_until(lambda:data()['workspace']==1 and data()['panes'][3]['focus'])
        assert data()['navigation_docked'] and not data()['drawer_open']
        key('super+alt+Up');wait_until(lambda:data()['workspace']==0 and data()['panes'][0]['focus'])
        click(*data()['agents_button']);wait_until(lambda:data()['navigation_section']=='agents')
        assert not data()['overview'] and not data()['navigation_agent_indices']
        capture('navigation-agents-empty.png')
        if wayland:run('niri','msg','action','set-window-width','680')
        else:cmd('windowsize','--sync',window,680,760)
        wait_until(lambda:data()['window_width']==680 and not data()['navigation_docked'])
        assert data()['navigation_pinned']
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);key('Escape');wait_until(lambda:not data()['drawer_open'])
        if wayland:run('niri','msg','action','set-window-width','1240')
        else:cmd('windowsize','--sync',window,1240,760)
        wait_until(lambda:data()['navigation_docked'])
        click(*data()['navigation_pin']);wait_until(lambda:not data()['navigation_pinned'] and data()['drawer_open']);key('Escape')
        wait_until(lambda:not data()['drawer_open'] and data()['panes'][0]['focus'] and sizes()==nav_sizes)
        assert [p['instance'] for p in data()['panes']]==nav_instances and received(0)==b'first'
        # Workspace arrows stay inside real entries; creation is a separate action.
        before_navigation=workspace_catalog.load(root);before_input=[received(i) for i in range(6)]
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        key('End');key('Down');key('Return');wait_until(lambda:not data()['drawer_open'] and data()['workspace']==0 and data()['panes'][0]['focus'])
        assert not data()['creation_open'] and workspace_catalog.load(root)==before_navigation
        assert [received(i) for i in range(6)]==before_input
        # Closing another row does not select it; Enter initially activates Cancel.
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['remove_workspace_1']);wait_until(lambda:data()['terminal_action_open'] and not data()['drawer_open'])
        assert data()['terminal_action_kind']=='workspace-close' and data()['terminal_action_target']=='workspace-1'
        assert data()['workspace_removal_context']=='Side project · 3 terminals' and data()['workspace']==0
        wait_until(lambda:data()['confirmation_cancel_focus']);capture('workspace-remove-confirm.png');key('Return');wait_until(lambda:not data()['terminal_action_open'] and data()['panes'][0]['focus'])
        assert workspace_catalog.load(root)==before_navigation and [received(i) for i in range(6)]==before_input
        # Focus/style changes must paint real cursor pixels without replacing readers.
        wait_until(lambda:data()['panes'][0]['cursor']=='block' and data()['panes'][1]['cursor']=='outline')
        capture('cursor-active.png');pixels=json.loads(Path(str(report)+'.cursor.json').read_text())
        assert pixels[0]['cursor_pixels']>pixels[0]['area']*.75,pixels[0]
        assert pixels[1]['cursor_pixels']<pixels[1]['area']*.5 and pixels[1]['background_pixels']>0,pixels[1]
        # A click focuses and clears; only a deliberate drag may select text.
        pane=data()['panes'][0];click(pane['x'],pane['y'])
        wait_until(lambda:not data()['panes'][0]['selection'])
        time.sleep(.12);assert not data()['panes'][0]['selection'],'single click left a selected block'
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
        key('super+alt+slash');wait_until(lambda:data()['panes'][0]['cursor']=='outline');capture('chrome-shortcuts.png')
        key('Escape');wait_until(lambda:data()['panes'][0]['cursor']=='block')
        assert sizes()==cursor_grid
        # Real chord recording is modal; drafts and failures never reach the PTY.
        shortcut_grid=sizes();shortcut_input=[received(i) for i in range(6)]
        def shortcut_settings():
            click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
            click(*data()['shortcut_settings_menu']);wait_until(lambda:data()['shortcut_settings_open'] and not data()['application_menu_open'])
        def record_key(value):
            click(*data()['shortcut_record']);wait_until(lambda:data()['shortcut_recording']);raw_key(value)
        shortcut_settings();record_key('F9');wait_until(lambda:bool(data()['shortcut_edit_error']))
        assert data()['shortcut_recording'] and data()['shortcut_sequences']['commands']=='F10'
        raw_key('ctrl+c');wait_until(lambda:'reserved' in data()['shortcut_edit_error'].lower())
        raw_key('Escape');wait_until(lambda:not data()['shortcut_recording'] and data()['shortcut_settings_open'])
        record_key('F7');wait_until(lambda:not data()['shortcut_recording'] and data()['shortcut_draft_labels']['commands']=='F7')
        capture('shortcut-editor.png');click(*data()['shortcut_cancel']);wait_until(lambda:not data()['shortcut_settings_open'])
        assert data()['shortcut_sequences']['commands']=='F10' and not (root/'ui/linux/shortcuts.conf').exists()
        shortcut_settings();record_key('F7');wait_until(lambda:not data()['shortcut_recording'])
        click(*data()['shortcut_apply']);wait_until(lambda:not data()['shortcut_settings_open'] and data()['shortcut_sequences']['commands']=='F7')
        raw_key('F7');wait_until(lambda:data()['commands_open']);raw_key('Escape');wait_until(lambda:not data()['commands_open'])
        shortcut_settings();click(*data()['shortcut_clear']);wait_until(lambda:not data()['shortcut_draft_labels']['commands'])
        click(*data()['shortcut_reset']);wait_until(lambda:data()['shortcut_draft_labels']['commands']=='F10')
        click(*data()['shortcut_apply']);wait_until(lambda:not data()['shortcut_settings_open'] and data()['shortcut_sequences']['commands']=='F10')
        assert (root/'ui/linux/shortcuts.conf').read_text()=='workbench-shortcuts-v1\n'
        assert managed.read_bytes()==managed_bytes and sizes()==shortcut_grid and [received(i) for i in range(6)]==shortcut_input
        # One-shot forwarding is tied to the focused reader and bypasses app chords.
        forwarding_input=received(0);forwarding_sizes=sizes()
        raw_key('F6');wait_until(lambda:data()['forwarding_armed']);capture('forward-key.png')
        raw_key('F10');wait_until(lambda:not data()['forwarding_armed'] and received(0)==forwarding_input+b'\x1b[21~')
        assert not data()['commands_open']
        raw_key('F10');wait_until(lambda:data()['commands_open']);key('Escape');wait_until(lambda:not data()['commands_open'])
        forwarding_input=received(0);raw_key('F6');wait_until(lambda:data()['forwarding_armed'])
        raw_key('Escape');wait_until(lambda:not data()['forwarding_armed']);time.sleep(.1);assert received(0)==forwarding_input
        raw_key('F6');wait_until(lambda:data()['forwarding_armed'])
        click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'] and not data()['forwarding_armed'])
        key('Escape');wait_until(lambda:not data()['application_menu_open'])
        # Native clipboard chords must go through the encoder once, not invoke Copy.
        raw_key('F6');wait_until(lambda:data()['forwarding_armed']);raw_key('ctrl+shift+c')
        wait_until(lambda:received(0)==forwarding_input+b'\x1b[99;6u' and not data()['forwarding_armed'])
        forwarding_input=received(0)
        # No key is sent by cancelling with the explicit control or by timeout.
        raw_key('F6');wait_until(lambda:data()['forwarding_armed']);click(*data()['forward_cancel'])
        wait_until(lambda:not data()['forwarding_armed']);assert received(0)==forwarding_input
        # Clicking another terminal cancels instead of retargeting the pending key.
        raw_key('F6');wait_until(lambda:data()['forwarding_armed'])
        target=data()['panes'][1];click(target['x'],target['y']);wait_until(lambda:data()['panes'][1]['focus'] and not data()['forwarding_armed'])
        target=data()['panes'][0];click(target['x'],target['y']);wait_until(lambda:data()['panes'][0]['focus'])
        raw_key('F6');wait_until(lambda:data()['forwarding_armed']);wait_until(lambda:not data()['forwarding_armed'],timeout=12)
        assert received(0)==forwarding_input
        # The menu arms the terminal after its popup has released keyboard focus.
        def forward_menu():
            click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
            click(*data()['forward_menu']);wait_until(lambda:data()['forwarding_armed'] and not data()['application_menu_open'])
        # Application-requested key-release reporting is retained when forwarding.
        (root/'0'/'cursor').write_bytes(b'\x1b[>3u');wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.15)
        forward_menu();raw_key('F10');wait_until(lambda:len(received(0))>len(forwarding_input));time.sleep(.15)
        assert received(0)[len(forwarding_input):]==b'\x1b[21~\x1b[21;1:3~',repr(received(0)[len(forwarding_input):])
        assert not data()['commands_open'] and not data()['forwarding_armed']
        (root/'0'/'cursor').write_bytes(b'\x1b[<u');wait_until(lambda:not (root/'0'/'cursor').exists());time.sleep(.15)
        assert sizes()==forwarding_sizes
        # Reset the raw fixture capture so the rest of the existing suite keeps its baseline.
        (root/'0'/'input').write_bytes(shortcut_input[0])
        # Palette matching/input is local: no shell, geometry or ownership changes.
        command_grid=sizes();command_pids=[p['instance'] for p in data()['panes']]
        command_input=[received(i) for i in range(6)]
        def command_search(query):
            if not data()['commands_open']:
                key('super+alt+p');wait_until(lambda:data()['commands_open'] and data()['command_search_focus'])
            key('ctrl+a');typed(query)
            wait_until(lambda:bool(data()['command_matches']) and all(all(word in (entry['title']+' '+entry['detail']).lower() for word in query.lower().split()) for entry in data()['command_matches']))
        def stop_all_flow():
            global window
            original_window=window;stop_peer=None;excluded_owner=None
            def focus(target):
                if wayland:run('niri','msg','action','focus-window','--id',target)
                else:cmd('windowraise',target);cmd('windowfocus',target)
            def pane(snapshot,identity):return next(p for p in snapshot['panes'] if p['identity']==identity)
            def open_stop(menu=False):
                if menu:
                    click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
                    click(*data()['session_stop_item'])
                else:command_search('stop all terminals');key('Return')
                wait_until(lambda:data()['terminal_action_open'] and data()['terminal_action_kind']=='session-stop' and data()['confirmation_cancel_focus'])
            base=workspace_catalog.load(root);count=len(data()['panes'])
            target=next(p['identity'] for p in data()['panes'] if p['ready'])
            destination=next(w['id'] for w in base['workspaces'] if len(w['terminals'])<8)
            original_objects={p['identity']:p['instance'] for p in data()['panes']}
            # Cancel/initial Enter preserves every runtime and its reader.
            open_stop(menu=True);frozen=list(data()['session_stop_ids']);assert len(frozen)==count
            key('Return');wait_until(lambda:not data()['terminal_action_open'])
            assert workspace_catalog.load(root)==base and all(p['instance']==original_objects[p['identity']] for p in data()['panes'])
            # A removed identity disables confirmation; no successor can be targeted.
            transient=request(root,Path(runtime),root,'terminal.create',destination,'Shell')['data']['workspaces']
            transient=next(t['id'] for w in transient for t in w['terminals'] if t['id'] not in original_objects)
            wait_until(lambda:len(data()['panes'])==count+1)
            open_stop();request(root,Path(runtime),root,'terminal.close',transient)
            wait_until(lambda:not data()['session_stop_valid'] and not data()['confirm_terminal_action_enabled'])
            click(*data()['confirm_terminal_action']);time.sleep(.1);assert data()['terminal_action_open']
            capture('session-stop-stale.png');key('Escape');wait_until(lambda:not data()['terminal_action_open'] and len(data()['panes'])==count)
            # A second window owns one confirmed terminal; stop applies across owners.
            peer_report=artifacts/'stop-peer.json';peer_report.unlink(missing_ok=True)
            peer_config=root/'stop-peer.json';settings=json.loads(config.read_text());settings['catalog']=workspace_catalog.load(root);settings['view_state']=str(root/'stop-peer-view.json');peer_config.write_text(json.dumps(settings))
            stop_peer=subprocess.Popen([viewer,str(peer_config),'--test-report',str(peer_report)],env=env,stdout=log,stderr=log);processes.append(stop_peer)
            def peer_data():
                try:return json.loads(peer_report.read_text())
                except (FileNotFoundError,ValueError):return {}
            try:
                wait_until(lambda:len(peer_data().get('panes',[]))==count and pane(peer_data(),target)['attachment']=='elsewhere',timeout=20)
                if wayland:
                    peer_window=next(w['id'] for w in json.loads(run('niri','msg','-j','windows')) if w['title']=='Terminal Workbench · Qt UI study' and w['id']!=original_window)
                    run('niri','msg','action','focus-window','--id',peer_window);run('niri','msg','action','move-window-to-floating')
                else:peer_window=next(w for w in cmd('search','--name','^Terminal Workbench · Qt UI study$').splitlines() if w!=original_window)
                # Show the first workspace/terminal containing the target in the peer.
                window=peer_window;focus(window)
                for _ in range(8):key('super+alt+Up')
                for _ in range(8):key('super+alt+Left')
                wait_until(lambda:pane(peer_data(),target)['lifecycle'].get('state')=='running' and not pane(peer_data(),target)['lifecycle'].get('busy'))
                click(*pane(peer_data(),target)['primary_action']);wait_until(lambda:peer_data()['terminal_action_open'])
                click(*peer_data()['confirm_terminal_action']);wait_until(lambda:pane(peer_data(),target)['ready'] and not peer_data()['terminal_action_open'])
                window=original_window;focus(window);wait_until(lambda:pane(data(),target)['attachment']=='elsewhere')
                open_stop();frozen=list(data()['session_stop_ids']);assert len(frozen)==count
                added=request(root,Path(runtime),root,'terminal.create',destination,'Shell')['data']
                excluded=next(t['id'] for w in added['workspaces'] for t in w['terminals'] if t['id'] not in frozen)
                wait_until(lambda:len(data()['panes'])==count+1 and len(peer_data()['panes'])==count+1)
                assert data()['session_stop_ids']==frozen and excluded not in frozen
                excluded_owner=snapshot_attach(root/(excluded+'.sock'),takeover=True);collect(excluded_owner)
                marker=root/'excluded-before';after=root/'excluded-after'
                send(excluded_owner,16,('WB_EXCLUDED=survives; printf "%s:%s" "$$" "$WB_EXCLUDED" > '+shlex.quote(str(marker))+'\n').encode());wait_until(lambda:marker.exists() and marker.read_text())
                inode=(root/(excluded+'.sock')).stat().st_ino
                before_stop=workspace_catalog.load(root);capture('session-stop-confirm.png')
                click(*data()['confirm_terminal_action']);wait_until(lambda:not data()['terminal_action_open'],timeout=30)
                wait_until(lambda:all(not pane(data(),identity)['ready'] and pane(data(),identity)['lifecycle'].get('state')=='stopped' for identity in frozen),timeout=20)
                wait_until(lambda:all(not pane(peer_data(),identity)['ready'] and pane(peer_data(),identity)['lifecycle'].get('state')=='stopped' for identity in frozen),timeout=20)
                assert workspace_catalog.load(root)==before_stop and not data()['error']
                assert all(pane(data(),identity)['instance']==original_objects[identity] for identity in frozen)
                send(excluded_owner,16,('printf "%s:%s" "$$" "$WB_EXCLUDED" > '+shlex.quote(str(after))+'\n').encode());wait_until(lambda:after.exists() and after.read_text())
                assert after.read_text()==marker.read_text() and (root/(excluded+'.sock')).stat().st_ino==inode,'excluded shell state/PID changed'
                capture('session-stop-complete.png')
                # Explicit recovery starts only the chosen entry; peers observe its state.
                for _ in range(8):key('super+alt+Up')
                for _ in range(8):key('super+alt+Left')
                click(*pane(data(),target)['primary_action']);wait_until(lambda:pane(data(),target)['ready'],timeout=15)
                wait_until(lambda:pane(peer_data(),target)['lifecycle'].get('state')=='running',timeout=15)
                assert all(not pane(data(),identity)['ready'] for identity in frozen if identity!=target)
                window=peer_window;focus(window)
                if wayland:run('niri','msg','action','close-window','--id',window)
                else:close_window(window,env)
                stop_peer.wait(timeout=5);assert stop_peer.returncode==0
                window=original_window;focus(window)
                assert request(root,Path(runtime),root,'terminal.status',target)['state']=='running' and request(root,Path(runtime),root,'terminal.status',excluded)['state']=='running','closing peer stopped a surviving shell'
                excluded_owner.close();excluded_owner=None
                request(root,Path(runtime),root,'terminal.close',excluded);wait_until(lambda:len(data()['panes'])==count)
                assert workspace_catalog.load(root)==base
            finally:
                window=original_window
                if excluded_owner:excluded_owner.close()
                if stop_peer and stop_peer.poll() is None:stop_peer.terminate();stop_peer.wait(timeout=5)
            print(('wayland' if wayland else 'xcb')+' PASS: confirmed session stop, Cancel/initial Enter, stale identities, cross-window ownership/status, frozen-ID exclusion with same PID/state, preserved catalog/readers, explicit recovery and close-detach')
        if '--shutdown-only' in sys.argv[3:]:
            stop_all_flow()
            if wayland:run('niri','msg','action','close-window','--id',window)
            else:close_window(window,env)
            client.wait(timeout=5);assert client.returncode==0
            assert 'Qt UI: qrc:' not in (artifacts/'run.log').read_text(),'QML warning in shutdown regression'
            sys.exit(0)
        click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
        click(*data()['commands_menu_item']);wait_until(lambda:data()['commands_open'] and data()['command_search_focus'] and not data()['application_menu_open'])
        typed('definitely-no-such-command');wait_until(lambda:not data()['command_matches'])
        key('Return');key('Down');key('Up');time.sleep(.1)
        assert data()['commands_open'] and not data()['creation_open']
        key('Escape');wait_until(lambda:not data()['commands_open'] and data()['panes'][0]['focus'])
        command_search('terminal');wait_until(lambda:len(data()['command_matches'])>4)
        key('Down');wait_until(lambda:data()['command_current']==1)
        key('Up');wait_until(lambda:data()['command_current']==0)
        capture('commands-search.png');key('Escape');wait_until(lambda:not data()['commands_open'])
        command_search('reduce motion');wait_until(lambda:len(data()['command_first_row'])==2);click(*data()['command_first_row']);wait_until(lambda:not data()['commands_open'] and data()['reduced_motion'])
        command_search('enable animations');key('Return');wait_until(lambda:not data()['commands_open'] and not data()['reduced_motion'])
        command_search('terminal');key('Tab');wait_until(lambda:not data()['command_search_focus'])
        key('shift+Tab');wait_until(lambda:data()['command_search_focus'])
        key('Tab');key('Tab');key('Return');wait_until(lambda:not data()['commands_open'] and data()['panes'][0]['focus'])
        command_search('previous terminal');assert not data()['command_matches'][0]['available'] and data()['command_matches'][0]['reason']
        key('Return');time.sleep(.1);assert data()['commands_open'] and data()['selected']==0
        key('Escape');wait_until(lambda:not data()['commands_open'])
        command_search('find in terminal');key('Return');wait_until(lambda:data()['terminal_search_open'] and data()['terminal_search_focus'] and not data()['commands_open'])
        key('Escape');wait_until(lambda:not data()['terminal_search_open'] and data()['panes'][0]['focus'])
        command_search('new workspace');key('Return');wait_until(lambda:data()['creation_open'] and not data()['commands_open'] and data()['name_focus'])
        assert data()['workspace_count']==3 and data()['name_input']=='','palette Enter submitted the next dialog'
        key('Escape');wait_until(lambda:not data()['creation_open'])
        command_search('close terminal');key('Return');wait_until(lambda:data()['terminal_action_open'] and not data()['commands_open'])
        time.sleep(.1);assert len(data()['panes'])==6 and all(p['ready'] for p in data()['panes'])
        key('Escape');wait_until(lambda:not data()['terminal_action_open'] and data()['panes'][0]['focus'])
        command_search('show overview');key('Return');wait_until(lambda:data()['overview'] and not data()['commands_open'])
        command_search('workspace appearance');wait_until(lambda:len(data()['command_matches'])==1)
        assert not data()['command_matches'][0]['available']
        key('Return');time.sleep(.1);assert data()['commands_open'] and data()['overview'] and not data()['appearance_open']
        key('Escape');wait_until(lambda:not data()['commands_open']);assert data()['overview'],'palette Escape also closed Overview'
        key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][0]['focus'])
        assert sizes()==command_grid and [p['instance'] for p in data()['panes']]==command_pids
        assert [received(i) for i in range(6)]==command_input,'palette keys leaked into a PTY'
        # Preview is a window transaction; all panes retain their items and PTYs.
        font_grid=sizes();font_items=[p['instance'] for p in data()['panes']];font_input=[received(i) for i in range(6)]
        assert data()['font_defaults']=={'family':'Fira Code','size':16}
        assert 'Qt UI font settings:' not in (artifacts/'run.log').read_text(),'unused theme triggered font fallback'
        def settled_font(family,size):
            return data()['font_preferences']=={'family':family,'size':size} and all(p['font']=={'family':family,'size':size} and p['geometry_ready'] for p in data()['panes'])
        def font_menu():
            click(*data()['application_menu_button']);wait_until(lambda:data()['application_menu_open'])
            click(*data()['font_menu_item']);wait_until(lambda:data()['font_settings_open'] and not data()['application_menu_open'])
        font_menu();key('super+alt+t');key('super+alt+Right');time.sleep(.1)
        assert len(data()['panes'])==6 and data()['selected']==0,'global shortcut escaped font dialog'
        click(*data()['font_plus']);wait_until(lambda:settled_font('Fira Code',17))
        assert sizes()!=font_grid,'text size did not change the real PTY grid'
        click(*data()['font_jetbrains']);wait_until(lambda:settled_font('JetBrains Mono',17));capture('font-preview.png')
        key('Escape');wait_until(lambda:not data()['font_settings_open'] and settled_font('Fira Code',16) and sizes()==font_grid and data()['panes'][0]['focus'])
        assert not (root/'ui/linux/terminal-font.json').exists(),'cancel persisted font preview'
        command_search('bundled font');key('Return');wait_until(lambda:data()['font_settings_open'] and not data()['commands_open'])
        click(*data()['font_jetbrains']);click(*data()['font_plus']);wait_until(lambda:settled_font('JetBrains Mono',17))
        click(*data()['font_apply']);wait_until(lambda:not data()['font_settings_open'] and settled_font('JetBrains Mono',17))
        wait_until(lambda:all(p['requested_family']=='JetBrains Mono' and p['requested_points']==17 for p in display_data().get('panes',[])) and len(display_data().get('panes',[]))==6)
        font_layer=root/'ui/linux/terminal-font.json';font_saved=font_layer.read_bytes()
        assert json.loads(font_saved)=={'version':1,'family':'JetBrains Mono','size':17}
        font_menu();click(*data()['font_defaults_button']);wait_until(lambda:settled_font('Fira Code',16))
        key('Escape');wait_until(lambda:not data()['font_settings_open'] and settled_font('JetBrains Mono',17))
        assert font_layer.read_bytes()==font_saved,'cancel changed saved font'
        key('super+alt+0');wait_until(lambda:settled_font('JetBrains Mono',16))
        key('super+alt+equal');wait_until(lambda:settled_font('JetBrains Mono',17))
        key('super+alt+minus');wait_until(lambda:settled_font('JetBrains Mono',16))
        font_menu();click(*data()['font_defaults_button']);wait_until(lambda:settled_font('Fira Code',16))
        click(*data()['font_apply']);wait_until(lambda:not data()['font_settings_open'] and sizes()==font_grid and data()['panes'][0]['focus'])
        assert json.loads(font_layer.read_text())=={'version':1} and not data()['font_edit_error']
        assert [p['instance'] for p in data()['panes']]==font_items and [received(i) for i in range(6)]==font_input
        assert managed.read_bytes()==managed_bytes,'font UI rewrote managed settings'
        for i,pid in enumerate(pids):assert (root/str(i)/'pid').read_text()==pid;os.kill(int(pid),0)
        # Two Qt clients project shared metadata without transferring existing owners.
        peer_catalog=workspace_catalog.load(root);peer_instances={p['identity']:p['instance'] for p in data()['panes']};peer_input=[received(i) for i in range(6)]
        peer_report=artifacts/'peer-report.json';peer_report.unlink(missing_ok=True)
        peer_config=root/'peer-config.json';peer_settings=json.loads(config.read_text());peer_settings.update(saved_session=True,view_state=str(root/'peer-view.json'))
        peer_settings['catalog']=peer_catalog;peer_config.write_text(json.dumps(peer_settings))
        peer=subprocess.Popen([viewer,str(peer_config),'--test-report',str(peer_report)],env=env,stdout=log,stderr=log);processes.append(peer)
        def peer_data():
            try:return json.loads(peer_report.read_text())
            except (FileNotFoundError,ValueError):return {}
        wait_until(lambda:len(peer_data().get('panes',[]))==6 and all(p['attachment']=='elsewhere' for p in peer_data()['panes']),timeout=30)
        if wayland:run('niri','msg','action','focus-window','--id',window)
        else:cmd('windowraise',window);cmd('windowfocus',window)
        raw_key('F8');wait_until(lambda:data()['creation_open'] and data()['name_focus']);key('ctrl+a');typed('Peer renamed');key('Return')
        wait_until(lambda:not data()['creation_open'] and peer_data().get('workspace_names',[None])[0]=='Peer renamed')
        request(root,Path(runtime),root,'workspace.rename','workspace-0','Workbench')
        wait_until(lambda:data()['workspace_names'][0]=='Workbench' and peer_data()['workspace_names'][0]=='Workbench')
        request(root,Path(runtime),root,'workspace.directory','workspace-0',project_directory=str(chosen))
        wait_until(lambda:data()['workspace_directories'][0]==str(chosen) and peer_data()['workspace_directories'][0]==str(chosen))
        request(root,Path(runtime),root,'workspace.directory','workspace-0',project_directory=str(root))
        request(root,Path(runtime),root,'terminal.create','workspace-0','Peer shell')
        wait_until(lambda:len(data()['panes'])==7 and len(peer_data()['panes'])==7)
        extra=next(p['identity'] for p in data()['panes'] if p['identity'] not in peer_instances)
        request(root,Path(runtime),root,'terminal.move','terminal-1',destination='workspace-1',position=1)
        wait_until(lambda:data()['panes'][1]['workspace']==1 and peer_data()['panes'][1]['workspace']==1)
        request(root,Path(runtime),root,'terminal.move','terminal-1',destination='workspace-0',position=1)
        request(root,Path(runtime),root,'terminal.close',extra)
        wait_until(lambda:len(data()['panes'])==6 and len(peer_data()['panes'])==6 and data()['panes'][1]['workspace']==0)
        # Confirm removal by stable ID while its label changes in another client.
        removing=request(root,Path(runtime),root,'workspace.create_at',title='Remove target',project_directory=str(root))['data']['workspaces'][-1]
        removal_id=removing['id'];removal_terminal=removing['terminals'][0]['id']
        wait_until(lambda:data()['workspace_count']==4 and peer_data()['workspace_count']==4)
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['remove_workspace_3']);wait_until(lambda:data()['terminal_action_open'] and not data()['drawer_open'])
        request(root,Path(runtime),root,'workspace.rename',removal_id,'Renamed removal target')
        wait_until(lambda:data()['workspace_removal_context']=='Renamed removal target · 1 terminal')
        key('super+alt+Down');key('super+alt+t');time.sleep(.1)
        assert data()['workspace']==0 and data()['terminal_action_target']==removal_id and len(data()['panes'])==7
        click(*data()['confirm_terminal_action']);wait_until(lambda:not data()['terminal_action_open'] and data()['workspace_count']==3 and peer_data()['workspace_count']==3)
        assert data()['workspace']==0 and not (root/(removal_terminal+'.sock')).exists()
        # External deletion dismisses the confirmation without targeting its successor.
        removed=request(root,Path(runtime),root,'workspace.create_at',title='Deleted elsewhere',project_directory=str(root))['data']['workspaces'][-1]['id']
        wait_until(lambda:data()['workspace_count']==4)
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['remove_workspace_3']);wait_until(lambda:data()['terminal_action_open'] and not data()['drawer_open'])
        request(root,Path(runtime),root,'workspace.close',removed)
        wait_until(lambda:not data()['terminal_action_open'] and data()['workspace_count']==3 and peer_data()['workspace_count']==3)
        # A foreign deletion closes an editor for that ID, without retargeting it.
        result=request(root,Path(runtime),root,'workspace.create_at',title='Temporary peer',project_directory=str(root))['data']
        removed=result['workspaces'][-1]['id'];wait_until(lambda:data()['workspace_count']==4 and peer_data()['workspace_count']==4)
        key('super+alt+Down');key('super+alt+Down');key('super+alt+Down');wait_until(lambda:data()['workspace']==3)
        raw_key('F8');wait_until(lambda:data()['creation_open']);request(root,Path(runtime),root,'workspace.close',removed)
        wait_until(lambda:data()['workspace_count']==3 and not data()['creation_open'] and peer_data()['workspace_count']==3)
        key('super+alt+Up');key('super+alt+Up');wait_until(lambda:data()['workspace']==0 and data()['selected']==0 and data()['panes'][0]['focus'])
        assert workspace_catalog.load(root)==peer_catalog
        assert {p['identity']:p['instance'] for p in data()['panes']}==peer_instances and all(p['ready'] for p in data()['panes'])
        assert [received(i) for i in range(6)]==peer_input and not data()['catalog_notice'] and not peer_data()['catalog_notice']
        capture('catalog-peer-sync.png');peer.terminate();peer.wait(timeout=5)
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
            if not (artifacts/'chrome-menu.png').exists():capture('chrome-menu.png')
            click(*data()['appearance_menu_item']);wait_until(lambda:data()['appearance_open'] and data()['theme_search_focus'])
        def theme_state(index,background,red):
            value=data()
            return value['panes'][3]['sample_green']=='#112233' and value['chrome_background']==background and all(p['theme']==index and p['background']==background and p['sample_red']==red and p['sample_rgb']=='#010203' for p in value['panes'][3:6])
        appearance();click(*data()['theme_help_button']);wait_until(lambda:data()['theme_help_open']);capture('theme-help.png')
        key('Escape');wait_until(lambda:not data()['theme_help_open'] and data()['appearance_open'])
        click(*data()['theme_search_field']);typed('Workbench Dawn');wait_until(lambda:data()['theme_matches']==1);key('Down')
        wait_until(lambda:theme_state(1,'#f4f1eb','#a93f4a'))
        assert all(p['theme']==0 and p['background']=='#15191f' for p in data()['panes'][:3])
        assert sizes()==theme_sizes;capture('theme-light-preview.png')
        key('Escape');wait_until(lambda:not data()['appearance_open'] and data()['panes'][4]['focus'] and theme_state(0,'#15191f','#e49b9b'))
        appearance();typed('Dracula');wait_until(lambda:data()['theme_matches']==1);key('Down')
        wait_until(lambda:theme_state(2,'#282a36','#ff5555'));key('Return')
        wait_until(lambda:not data()['appearance_open'] and data()['panes'][4]['focus'])
        assert theme_state(2,'#282a36','#ff5555') and sizes()==theme_sizes
        assert (root/'workspaces.json').read_bytes()==theme_catalog_before,'appearance mutated runtime organization'
        # Returning to the managed default is a draft until Apply; explicit
        # project themes stay unchanged when the managed default changes.
        appearance();click(*data()['theme_default_button']);wait_until(lambda:theme_state(0,'#15191f','#e49b9b'))
        key('Escape');wait_until(lambda:not data()['appearance_open'] and theme_state(2,'#282a36','#ff5555'))
        appearance();click(*data()['theme_default_button']);click(*data()['theme_apply_button'])
        wait_until(lambda:not data()['appearance_open'] and theme_state(0,'#15191f','#e49b9b') and data()['workspace_themes'][1]==-1)
        managed.write_bytes(managed_bytes.replace(b'terminal-theme = Workbench Night',b'terminal-theme = Workbench Dawn'))
        wait_until(lambda:theme_state(1,'#f4f1eb','#a93f4a') and all(p['theme']==1 for p in data()['panes'][:3]))
        appearance();typed('Dracula');wait_until(lambda:data()['theme_matches']==1);key('Down');key('Return');wait_until(lambda:not data()['appearance_open'] and theme_state(2,'#282a36','#ff5555'))
        managed.write_bytes(managed_bytes)
        wait_until(lambda:data()['theme_default_index']==0 and all(p['theme']==0 for p in data()['panes'][:3]))
        assert theme_state(2,'#282a36','#ff5555'),'managed reload overwrote explicit workspace theme'
        appearance();typed('no-theme-matches-this');wait_until(lambda:data()['theme_matches']==0);capture('theme-no-results.png');key('Escape')
        wait_until(lambda:not data()['appearance_open'] and theme_state(2,'#282a36','#ff5555'))
        # Drop a TerminalColors .default file; opening Appearance discovers it.
        drop_file=root/'themes'/'folder-fixture.default';drop_bytes=b'background = #214365\nforeground = #e0e6ef\npalette = 1=#aa1133\ncommand = /never/execute\n'
        drop_file.write_bytes(drop_bytes);(root/'themes'/'invalid.default').write_text('background = #invalid\n')
        appearance();typed('Folder Fixture');wait_until(lambda:data()['theme_matches']==1 and data()['theme_catalog_count']==8)
        key('Down');wait_until(lambda:data()['chrome_background']=='#214365')
        folder_index=data()['panes'][3]['theme'];assert theme_state(folder_index,'#214365','#aa1133')
        capture('theme-directory-preview.png');key('Escape');wait_until(lambda:not data()['appearance_open'] and theme_state(2,'#282a36','#ff5555'))
        appearance();typed('Folder Fixture');key('Down');wait_until(lambda:data()['chrome_background']=='#214365');key('Return')
        wait_until(lambda:not data()['appearance_open'] and theme_state(folder_index,'#214365','#aa1133'))
        assert drop_file.read_bytes()==drop_bytes and not theme_store.exists() and sizes()==theme_sizes
        drop_file.unlink()
        # Save pin preference with the existing layout/theme view profile.
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['navigation_pin']);wait_until(lambda:data()['navigation_docked'] and not data()['drawer_open'])
        # Save a non-default layout/Overview and an immutable color snapshot.
        key('super+alt+r');key('super+alt+f');wait_until(lambda:data()['panes'][4]['columns']>data()['panes'][3]['columns'])
        command_search('reduce motion');key('Return');wait_until(lambda:data()['reduced_motion'])
        key('super+alt+o');wait_until(lambda:data()['overview'])
        def saved_view():
            try:return json.loads((root/'qt-view.json').read_text())
            except (FileNotFoundError,ValueError):return {}
        wait_until(lambda:saved_view().get('overview') and saved_view()['terminals']['terminal-4']['full'] and saved_view()['motion'] and saved_view().get('navigation_pinned'))
        preserved_input=[received(i) for i in range(6)];saved_geometry=sizes()
        client.terminate();client.wait(timeout=5)
        # Unknown old identities and JSON member order must not retarget state.
        saved=saved_view();saved['terminals']['removed-terminal']={'width':1/3,'full':False}
        saved['terminals']=dict(reversed(list(saved['terminals'].items())))
        (root/'qt-view.json').write_text(json.dumps(saved));report.unlink(missing_ok=True)
        client=subprocess.Popen([viewer,str(config),'--test-report',str(report)],env=env,stdout=log,stderr=log);processes.append(client)
        wait_until(ready_ui,timeout=30)
        if wayland:
            window=wait_until(lambda:next((w['id'] for w in json.loads(run('niri','msg','-j','windows')) if w['title']=='Terminal Workbench · Qt UI study'),None))
            run('niri','msg','action','focus-window','--id',window);run('niri','msg','action','move-window-to-floating')
            run('niri','msg','action','set-window-width','1240');run('niri','msg','action','set-window-height','760');run('niri','msg','action','move-floating-window','--x','70','--y','50')
        else:
            window=cmd('search','--sync','--name','^Terminal Workbench · Qt UI study$').splitlines()[0];cmd('windowfocus',window)
        wait_until(lambda:data()['overview'] and data()['workspace']==1 and data()['selected']==4 and data()['reduced_motion'] and sizes()==saved_geometry)
        assert data()['navigation_docked'] and not data()['view_error'] and data()['chrome_background']=='#214365'
        folder_index=data()['panes'][3]['theme'];assert theme_state(folder_index,'#214365','#aa1133')
        managed.write_bytes(managed_bytes.replace(b'terminal-theme = Workbench Night',b'terminal-theme = Workbench Dawn'))
        wait_until(lambda:data()['theme_default_index']==1 and all(p['theme']==1 for p in data()['panes'][:3]))
        assert theme_state(folder_index,'#214365','#aa1133'),'restore lost the distinction between inherited and explicit themes'
        managed.write_bytes(managed_bytes)
        wait_until(lambda:data()['theme_default_index']==0 and all(p['theme']==0 for p in data()['panes'][:3]))
        assert [received(i) for i in range(6)]==preserved_input
        for i,pid in enumerate(pids):assert (root/str(i)/'pid').read_text()==pid;os.kill(int(pid),0)
        capture('view-restored.png');key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][4]['focus'])
        click(*data()['navigation_pin']);wait_until(lambda:not data()['navigation_pinned'] and data()['drawer_open']);key('Escape');wait_until(lambda:not data()['drawer_open'])
        key('super+alt+f');key('super+alt+r');key('super+alt+r')
        command_search('enable animations');key('Return');wait_until(lambda:not data()['reduced_motion'] and sizes()==theme_sizes)
        appearance();wait_until(lambda:data()['theme_catalog_count']==8)
        assert theme_state(folder_index,'#214365','#aa1133'),'removing a file changed the active terminal colors'
        typed('Dracula');wait_until(lambda:data()['theme_matches']==1);key('Down');key('Return')
        wait_until(lambda:not data()['appearance_open'] and theme_state(2,'#282a36','#ff5555'))
        key('super+alt+Up');wait_until(lambda:data()['workspace']==0 and data()['chrome_background']=='#15191f')
        key('super+alt+Down');wait_until(lambda:data()['workspace']==1 and data()['chrome_background']=='#282a36')
        # Actual native overview drags use existing IDs without recreating delegates.
        drag_sizes=sizes();catalog_before=workspace_catalog.load(root)
        def order(w):return [t['id'] for t in workspace_catalog.load(root)['workspaces'][w]['terminals']]
        key('super+alt+o');wait_until(lambda:data()['overview']);time.sleep(.35)
        last=data()['panes'][2]
        drag(1,last['right']-8,last['y'],cancel=True)
        assert data()['overview'] and workspace_catalog.load(root)==catalog_before,'cancel committed a move'
        drag(1,last['right']-8,last['y'])
        wait_until(lambda:order(0)==['terminal-0','terminal-2','terminal-1'] and data()['panes'][1]['ordinal']==2)
        assert sizes()==drag_sizes,'reorder resized a PTY'
        key('Escape');wait_until(lambda:not data()['overview'])
        key('super+alt+Up');wait_until(lambda:data()['workspace']==0 and data()['selected']==1)
        key('super+alt+Left');wait_until(lambda:data()['selected']==2 and data()['panes'][2]['focus'])
        key('super+alt+Right');wait_until(lambda:data()['selected']==1 and data()['panes'][1]['focus'])
        key('super+alt+Down');wait_until(lambda:data()['workspace']==1 and data()['selected']==4)
        key('super+alt+o');wait_until(lambda:data()['overview']);time.sleep(.3)
        time.sleep(.3);middle=data()['panes'][2]
        drag(1,middle['left']+5,middle['y'])
        wait_until(lambda:order(0)==['terminal-0','terminal-1','terminal-2'])
        time.sleep(.3)
        # Empty workspace is a drop target, and moving back preserves its shell cwd.
        drag_workspace(0,2)
        wait_until(lambda:order(2)==['terminal-0'] and data()['panes'][0]['workspace']==2)
        assert sizes()==drag_sizes
        time.sleep(.3);drag_workspace(0,0)
        wait_until(lambda:order(0)==['terminal-0','terminal-1','terminal-2'] and data()['panes'][0]['workspace']==0)
        assert workspace_catalog.load(root)==catalog_before,'restoring the move changed catalog metadata'
        time.sleep(.3)
        # Destination workspace appearance applies; width, PID and input stay attached.
        target=data()['panes'][5];drag(1,target['right']-8,target['y'])
        wait_until(lambda:data()['panes'][1]['workspace']==1 and data()['panes'][1]['theme']==2)
        assert order(1)==['terminal-3','terminal-4','terminal-5','terminal-1'] and sizes()==drag_sizes
        capture('drag-moved.png')
        # Extra destination row pushes later groups offscreen; edge-scroll reaches them.
        time.sleep(.35);show_preview(1);pane=data()['panes'][1]
        move(pane['x'],(max(60,pane['top'])+min(748,pane['bottom']))/2);button('mousedown')
        try:
            move(150,740);wait_until(lambda:data()['drag_slot']==1)
            wait_until(lambda:data()['drop_space']==2,timeout=5)
        finally:button('mouseup')
        wait_until(lambda:data()['panes'][1]['workspace']==2,timeout=5)
        assert order(2)==['terminal-1'] and data()['panes'][1]['theme']==0
        # Back to the top, then restore the original organization.
        # A native wheel scroll changes only the Overview viewport.
        for _ in range(12):click(1100,180,4)
        wait_until(lambda:data()['content_y']<1);time.sleep(.3)
        # Source is below view: bring it into view first, then drag to the upper edge.
        for _ in range(10):click(1100,500,5)
        wait_until(lambda:data()['content_y']>100);time.sleep(.3)
        pane=data()['panes'][1]
        move(pane['x'],(max(60,pane['top'])+min(748,pane['bottom']))/2);button('mousedown')
        try:
            move(pane['x']+20,pane['y']);wait_until(lambda:data()['drag_slot']==1)
            move(100,65);wait_until(lambda:data()['content_y']<1,timeout=5)
            move(450,180);wait_until(lambda:data()['drop_space']==0)
        finally:button('mouseup')
        wait_until(lambda:order(0)==['terminal-0','terminal-1','terminal-2'] and data()['panes'][1]['workspace']==0)
        assert workspace_catalog.load(root)==catalog_before and sizes()==drag_sizes
        key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][4]['focus'])
        # Shift extends the same navigation chord to identity-preserving pane moves.
        keyboard_catalog=workspace_catalog.load(root);keyboard_sizes=sizes()
        key('super+alt+shift+Right')
        wait_until(lambda:order(1)==['terminal-3','terminal-5','terminal-4'] and data()['panes'][4]['focus'])
        key('super+alt+shift+Left');wait_until(lambda:workspace_catalog.load(root)==keyboard_catalog and not data()['busy'])
        key('super+alt+shift+Up')
        wait_until(lambda:order(0)==['terminal-0','terminal-1','terminal-2','terminal-4'] and data()['workspace']==0 and data()['selected']==4 and data()['panes'][4]['focus'] and data()['panes'][4]['theme']==0)
        key('super+alt+shift+Down');wait_until(lambda:data()['workspace']==1 and data()['panes'][4]['focus'] and data()['panes'][4]['theme']==2)
        key('super+alt+shift+Left');wait_until(lambda:workspace_catalog.load(root)==keyboard_catalog and not data()['busy'])
        key('super+alt+shift+Down');wait_until(lambda:order(2)==['terminal-4'] and data()['workspace']==2 and data()['panes'][4]['focus'])
        key('super+alt+shift+Up');wait_until(lambda:data()['workspace']==1 and data()['panes'][4]['focus'])
        key('super+alt+shift+Left');wait_until(lambda:workspace_catalog.load(root)==keyboard_catalog and not data()['busy'])
        key('super+alt+Down');wait_until(lambda:data()['workspace']==2 and data()['selected']==-1)
        key('super+alt+Up');wait_until(lambda:data()['workspace']==1 and data()['selected']==4 and data()['panes'][4]['focus'])
        key('super+alt+o');wait_until(lambda:data()['overview'])
        key('super+alt+shift+Up');wait_until(lambda:data()['workspace']==0 and data()['overview'] and not data()['busy'])
        key('super+alt+shift+Down');wait_until(lambda:data()['workspace']==1 and data()['overview'] and not data()['busy'])
        key('super+alt+shift+Left');wait_until(lambda:workspace_catalog.load(root)==keyboard_catalog and not data()['busy'])
        key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][4]['focus'])
        assert sizes()==keyboard_sizes,'keyboard move resized a terminal'
        time.sleep(.3);click(*data()['panes'][4]['layout_button']);wait_until(lambda:data()['layout_menu_open'])
        click(*data()['move_right_item']);wait_until(lambda:order(1)==['terminal-3','terminal-5','terminal-4'] and not data()['layout_menu_open'] and data()['panes'][4]['focus'])
        key('super+alt+shift+Left');wait_until(lambda:workspace_catalog.load(root)==keyboard_catalog and not data()['busy'])
        key('super+alt+Up');wait_until(lambda:data()['workspace']==0)
        key('super+alt+Left');wait_until(lambda:data()['selected']==0)
        key('super+alt+shift+Left');key('super+alt+shift+Up')
        assert workspace_catalog.load(root)==keyboard_catalog,'boundary move wrapped or mutated organization'
        key('super+alt+Down');wait_until(lambda:data()['selected']==4 and data()['panes'][4]['focus'])
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
        click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3);capture('chrome-workspaces.png')
        click(*data()['new_workspace_button']);wait_until(lambda:data()['creation_open']);time.sleep(.3)
        assert data()['name_focus'],'closing the drawer stole dialog focus'
        typed('Project test');key('Tab');key('ctrl+a');typed(str(root/'missing'/'nested'))
        picker_catalog=workspace_catalog.load(root)
        # The existing folder field's Tab/Space opens Browse with native keyboard input.
        key('Tab');key('space')
        wait_until(lambda:data()['folder_browser_open'] and data()['folder_browser_location']==str(root))
        capture('folder-browser-create.png');key('Escape')
        wait_until(lambda:not data()['folder_browser_open'] and data()['creation_open'] and data()['folder_focus'])
        assert data()['folder_input']==str(root/'missing'/'nested') and data()['name_input']=='Project test'
        assert workspace_catalog.load(root)==picker_catalog,'picker cancellation changed catalog'
        browse_folder(root);choose_folder('Project folder')
        wait_until(lambda:data()['folder_input']==str(project))
        assert data()['creation_open'] and workspace_catalog.load(root)==picker_catalog,'Browse submitted creation'
        key('Return')
        wait_until(lambda:data()['workspace_count']==4 and len(data()['panes'])==8 and not data()['creation_open'] and data()['panes'][7]['ready'] and data()['panes'][7]['focus'])
        assert data()['workspace']==3
        marker2=root/'workspace-cwd';typed('pwd > '+shlex.quote(str(marker2)));key('Return');wait_until(marker2.exists)
        assert marker2.read_text().strip()==str(project)
        created_workspace=workspace_catalog.load(root)['workspaces'][-1]
        assert created_workspace['name']=='Project test' and created_workspace['directory']==str(project)
        # Empty workspace can create its first terminal without special handling.
        key('super+alt+Up');wait_until(lambda:data()['workspace']==2 and data()['selected']==-1)
        workspace_form('rename');assert data()['name_input']=='Empty workspace'
        typed(' edited');key('Return')
        wait_until(lambda:not data()['creation_open'] and data()['workspace_names'][2]=='Empty workspace edited')
        assert data()['selected']==-1,'editing an empty workspace created a selection'
        workspace_form('folder');assert data()['folder_input']==str(root)
        key('ctrl+a');typed(str(project));key('Return')
        wait_until(lambda:not data()['creation_open'] and data()['workspace_directories'][2]==str(project))
        assert len(data()['panes'])==8,'editing an empty workspace spawned a terminal'
        key('super+alt+t');wait_until(lambda:len(data()['panes'])==9 and data()['panes'][8]['ready'] and data()['panes'][8]['focus'])
        wait_until(lambda:all(p['geometry_ready'] for p in data()['panes']))
        creation_sizes=sizes();key('super+alt+o');wait_until(lambda:data()['overview']);time.sleep(.3)
        assert sizes()==creation_sizes,('Overview resized dynamically created terminals',creation_sizes,sizes())
        capture('created-overview.png');key('Escape');wait_until(lambda:not data()['overview'])
        capture('working.png')
        # Fill a destination through the UI: rejected drops must not mutate or detach.
        key('super+alt+Up');wait_until(lambda:data()['workspace']==1)
        for slot in range(9,13):
            key('super+alt+t');wait_until(lambda:len(data()['panes'])==slot+1 and data()['panes'][slot]['ready'] and not data()['busy'])
        wait_until(lambda:all(p['geometry_ready'] for p in data()['panes']))
        full_catalog=workspace_catalog.load(root);assert len(order(1))==8
        full_sizes=sizes();key('super+alt+o');wait_until(lambda:data()['overview']);time.sleep(.35)
        drag_workspace(0,1,allowed=False)
        wait_until(lambda:data()['drag_slot']==-1)
        assert workspace_catalog.load(root)==full_catalog and sizes()==full_sizes,'full destination accepted a drop'
        last=data()['panes'][5];drag(3,last['right']-8,last['y'])
        wait_until(lambda:order(1)[:3]==['terminal-4','terminal-5','terminal-3'])
        assert sizes()==full_sizes,'full workspace reorder resized a terminal'
        key('Escape');wait_until(lambda:not data()['overview'])
        key('super+alt+Down');wait_until(lambda:data()['workspace']==2 and data()['panes'][8]['focus'])
        empty_cwd=root/'empty-workspace-cwd';typed('pwd > '+shlex.quote(str(empty_cwd)));key('Return');wait_until(empty_cwd.exists)
        assert empty_cwd.read_text().strip()==str(project),'first shell ignored edited empty-workspace folder'
        key('super+alt+Down');wait_until(lambda:data()['workspace']==3 and data()['panes'][7]['focus'])
        editing_sizes=sizes();editing_catalog=workspace_catalog.load(root)
        workspace_form('rename');assert data()['name_input']=='Project test'
        typed(' renamed');key('Home');typed('Qt ')
        wait_until(lambda:data()['name_input']=='Qt Project test renamed')
        capture('workspace-rename.png');key('Return')
        wait_until(lambda:not data()['creation_open'] and data()['workspace_names'][3]=='Qt Project test renamed' and data()['panes'][7]['focus'])
        renamed=workspace_catalog.load(root)
        assert renamed['workspaces'][3]['name']=='Qt Project test renamed'
        assert renamed['workspaces'][3]['id']==editing_catalog['workspaces'][3]['id']
        assert renamed['workspaces'][3]['terminals']==editing_catalog['workspaces'][3]['terminals']
        workspace_form('rename');key('ctrl+a');key('BackSpace')
        wait_until(lambda:data()['name_input']=='' and not data()['form_submit_enabled'])
        key('Escape');wait_until(lambda:not data()['creation_open'] and data()['panes'][7]['focus'])
        assert workspace_catalog.load(root)==renamed,'cancelled rename mutated catalog'
        workspace_form('folder');assert data()['folder_input']==str(project)
        key('ctrl+a');typed(str(root/'missing'));key('Return')
        wait_until(lambda:'Folder' in data()['error'] and not data()['busy'])
        assert workspace_catalog.load(root)==renamed and data()['creation_open']
        capture('workspace-folder-error.png')
        key('Escape');wait_until(lambda:not data()['creation_open'])
        workspace_form('folder');assert data()['error']=='' and data()['folder_input']==str(project)
        typed('/missing/nested');browse_folder(project)
        choose_folder(chosen.name);wait_until(lambda:data()['folder_input']==str(chosen))
        assert data()['creation_open'] and workspace_catalog.load(root)==renamed,'Browse saved a folder without Save'
        capture('folder-browser-selected.png');key('Escape');wait_until(lambda:not data()['creation_open'])
        assert workspace_catalog.load(root)==renamed,'cancelled folder edit applied the picker choice'
        workspace_form('folder');assert data()['folder_input']==str(project)
        browse_folder(project);choose_folder(chosen.name)
        wait_until(lambda:data()['folder_input']==str(chosen))
        capture('workspace-folder.png');key('Return')
        wait_until(lambda:not data()['creation_open'] and data()['workspace_directories'][3]==str(chosen) and data()['panes'][7]['focus'])
        assert sizes()==editing_sizes,'workspace edit resized existing terminals'
        unchanged_cwd=root/'existing-shell-cwd';typed('pwd > '+shlex.quote(str(unchanged_cwd)));key('Return');wait_until(unchanged_cwd.exists)
        assert unchanged_cwd.read_text().strip()==str(project),'folder edit changed a running shell cwd'
        key('super+alt+t');wait_until(lambda:len(data()['panes'])==14 and data()['panes'][13]['ready'] and data()['panes'][13]['focus'])
        new_cwd=root/'edited-new-shell-cwd';typed('pwd > '+shlex.quote(str(new_cwd)));key('Return');wait_until(new_cwd.exists)
        assert new_cwd.read_text().strip()==str(chosen),'new shell ignored edited folder'
        assert data()['panes'][7]['theme']==0 and data()['panes'][13]['theme']==0
        # A full destination consumes the move shortcut without sending it to a PTY.
        key('super+alt+Up');wait_until(lambda:data()['workspace']==2 and data()['panes'][8]['focus'])
        full_move_catalog=workspace_catalog.load(root);full_move_sizes=sizes()
        key('super+alt+shift+Up');wait_until(lambda:'eight terminals' in data()['move_notice'])
        assert workspace_catalog.load(root)==full_move_catalog and data()['selected']==8
        capture('keyboard-move-full.png')
        key('super+alt+shift+Down');wait_until(lambda:data()['workspace']==3 and data()['selected']==8 and data()['panes'][8]['focus'])
        key('super+alt+shift+Up');wait_until(lambda:data()['workspace']==2 and data()['panes'][8]['focus'] and not data()['busy'])
        assert workspace_catalog.load(root)==full_move_catalog and sizes()==full_move_sizes
        expected=[b'first',b'second',b'',b'peer',b'chosen\x1b',b'']
        assert [received(i) for i in range(6)]==expected,'navigation/overview shortcuts leaked into a PTY'
        for i,pid in enumerate(pids):
            assert (root/str(i)/'pid').read_text()==pid;os.kill(int(pid),0)
            assert Path(f'/proc/{pid}/cwd').resolve()==root/str(i),'move changed an existing shell cwd'
        # Real runtime context and attention, independently observed from the PTY.
        metadata_catalog=workspace_catalog.load(root);metadata_sizes=sizes()
        repo=project/'Repo café';repo.mkdir()
        def git(*args):return subprocess.run(['git','-C',str(repo),*args],check=True,capture_output=True,text=True).stdout.strip()
        git('init','-q','-b','feature/context');git('-c','user.name=Test','-c','user.email=test@example.invalid','commit','-q','--allow-empty','-m','fixture')
        tree=project/'Linked tree';git('worktree','add','-q','-b','topic',str(tree))
        command='cd '+shlex.quote(str(tree))+'; echo $$ > '+shlex.quote(str(root/'metadata-shell-pid'))
        typed(command);key('Return');wait_until((root/'metadata-shell-pid').exists)
        wait_until(lambda:data()['panes'][8]['facts'].get('branch')=='topic' and data()['panes'][8]['facts'].get('worktree'))
        assert data()['panes'][8]['facts']['cwd']==str(tree)
        assert 'topic' in data()['panes'][8]['facts']['title'] and 'Linked tree' in data()['panes'][8]['facts']['title']
        capture('metadata-worktree.png')
        fixture=root/'codex';shutil.copyfile('/usr/bin/sleep',fixture);fixture.chmod(0o700)
        typed("sh -c "+shlex.quote('echo $$ > '+shlex.quote(str(root/'agent.pid'))+'; exec '+shlex.quote(str(fixture))+' 120'));key('Return')
        wait_until(lambda:data()['panes'][8]['facts'].get('agent')=='Codex')
        wait_until((root/'agent.pid').exists)
        group=os.getpgid(int((root/'agent.pid').read_text()))
        hook_env=dict(os.environ,WORKBENCH_SESSION_SOCKET=str(root/(data()['panes'][8]['identity']+'.sock')),WORKBENCH_AGENT_GROUP=str(group),WORKBENCH_AGENT_RUN=uuid.uuid4().hex)
        def hook(event,label):
            result=subprocess.run([sys.executable,str(ROOT/'scripts/agent-hook.py'),'codex'],input=json.dumps({'hook_event_name':event}),env=hook_env,text=True,capture_output=True,timeout=3)
            assert result.returncode==0 and not result.stdout and not result.stderr
            wait_until(lambda:data()['panes'][8]['facts'].get('state')==label)
        hook('UserPromptSubmit','Working')
        # The left agent list is separate from the spatial Overview.
        click(*data()['agents_button']);wait_until(lambda:data()['drawer_open'] and data()['navigation_agent_indices']==[8]);time.sleep(.3)
        assert not data()['overview']
        wait_until(lambda:any('Codex · Working' in label for label in data()['navigation_agent_labels']))
        capture('navigation-agent-working.png')
        hook('PermissionRequest','Needs input');wait_until(lambda:any('Codex · Needs input' in label for label in data()['navigation_agent_labels']))
        capture('navigation-agent-needs.png')
        hook('UserPromptSubmit','Working');wait_until(lambda:any('Codex · Working' in label for label in data()['navigation_agent_labels']))
        click(*data()['navigation_first_agent']);wait_until(lambda:not data()['drawer_open'] and data()['panes'][8]['focus'])
        key('super+alt+Up');wait_until(lambda:data()['workspace']==1)
        click(*data()['agents_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
        click(*data()['navigation_first_agent']);wait_until(lambda:not data()['drawer_open'] and data()['workspace']==2 and data()['selected']==8 and data()['panes'][8]['focus'])
        key('super+alt+o');wait_until(lambda:data()['overview'])
        click(*data()['agent_filter_button']);wait_until(lambda:data()['agents_only'] and data()['overview_matches']==1)
        assert [i for i,p in enumerate(data()['panes']) if p['visible']]==[8]
        assert sizes()==metadata_sizes,'metadata/filter presentation resized the PTY'
        click(*data()['state_filter_button']);wait_until(lambda:data()['needs_only'] and data()['overview_matches']==0)
        capture('metadata-no-needs.png')
        hook('PermissionRequest','Needs input');wait_until(lambda:data()['overview_matches']==1 and data()['waiting_count']==1)
        assert data()['panes'][8]['visible']
        time.sleep(.35)
        assert data()['panes'][8]['caption_visible'] and 'Codex · Needs input' in data()['panes'][8]['caption'],'native-size Overview caption missing'
        move(50,180);capture('metadata-needs-input.png')
        # Filtered drag maps the sole visible card back to the real ordinal.
        drag_workspace(8,2);wait_until(lambda:data()['drag_slot']==-1)
        assert workspace_catalog.load(root)==metadata_catalog,'filtered no-op reordered hidden terminals'
        hook('Stop','Ready');wait_until(lambda:data()['overview_matches']==0 and data()['waiting_count']==0)
        click(*data()['state_filter_button']);wait_until(lambda:not data()['needs_only'] and data()['overview_matches']==1)
        click(*data()['agent_filter_button']);wait_until(lambda:not data()['agents_only'] and data()['overview_matches']==14)
        key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][8]['focus'])
        key('ctrl+c');wait_until(lambda:data()['panes'][8]['facts'].get('agent')=='')
        typed('echo $$ > '+shlex.quote(str(root/'metadata-shell-after')));key('Return');wait_until((root/'metadata-shell-after').exists)
        assert (root/'metadata-shell-after').read_text()==(root/'metadata-shell-pid').read_text(),'metadata changed the shell owner'
        assert workspace_catalog.load(root)==metadata_catalog and sizes()==metadata_sizes
        # Lifecycle facts never guess an exit reason or start a replacement implicitly.
        life_catalog=workspace_catalog.load(root);life_ids={p['identity']:p['instance'] for p in data()['panes']}
        target_identity=data()['panes'][8]['identity'];target_socket=root/(target_identity+'.sock')
        typed('exit 42');key('Return');wait_until(lambda:not data()['panes'][8]['ready'])
        wait_until(lambda:request(root,Path(runtime),root,'terminal.lifecycle',target_identity)['state']=='stopped')
        click(*data()['panes'][8]['check_action'])
        wait_until(lambda:data()['panes'][8]['lifecycle'].get('state')=='stopped' and '42' in data()['panes'][8]['lifecycle'].get('message',''))
        capture('lifecycle-exit-42.png')
        stopped_attempts=data()['panes'][8]['attachment_attempts']
        raw_key('super+alt+s');wait_until(lambda:data()['terminal_search_focus'])
        typed('exit 42');wait_until(lambda:data()['terminal_search_state'].get('count',0)>0 and not data()['terminal_search_state']['busy'])
        capture('terminal-search-stopped.png');key('Escape');wait_until(lambda:not data()['terminal_search_open'])
        assert not data()['panes'][8]['ready'] and data()['panes'][8]['attachment_attempts']==stopped_attempts and request(root,Path(runtime),root,'terminal.lifecycle',target_identity)['state']=='stopped','search recovered or reattached a stopped shell'

        evidence=target_socket.with_suffix('.lifecycle');record=evidence.read_bytes();evidence.unlink()
        click(*data()['panes'][8]['check_action']);wait_until(lambda:data()['panes'][8]['lifecycle'].get('reason')=='unknown')
        assert 'no exit reason' in data()['panes'][8]['lifecycle']['message']
        evidence.write_bytes(record);evidence.chmod(0o600)
        click(*data()['panes'][8]['primary_action']);wait_until(lambda:data()['panes'][8]['ready'] and data()['panes'][8]['focus'])
        recovered_pid=root/'recovered-pid';typed('echo $$ > '+shlex.quote(str(recovered_pid)));key('Return');wait_until(recovered_pid.exists)
        assert recovered_pid.read_text()!=(root/'metadata-shell-pid').read_text()
        wait_until(lambda:data()['panes'][8]['facts'].get('cwd')==str(project))
        assert data()['panes'][8]['identity']==target_identity and data()['panes'][8]['instance']==life_ids[target_identity]
        assert workspace_catalog.load(root)==life_catalog
        # Real ownership revocation: only explicit Move here may take a live owner.
        peer=snapshot_attach(target_socket,takeover=True);collect(peer)
        try:
            wait_until(lambda:data()['panes'][8]['attachment']=='elsewhere' and data()['panes'][8]['lifecycle'].get('state')=='running')
            click(*data()['panes'][8]['primary_action']);wait_until(lambda:data()['terminal_action_open'] and data()['terminal_action_kind']=='move')
            key('Return');time.sleep(.1)  # Cancel has initial focus; Enter cannot confirm.
            if data()['terminal_action_open']:key('Escape')
            wait_until(lambda:not data()['terminal_action_open'])
            assert not data()['panes'][8]['ready']
            send(peer,16,b"printf WB_OWNER_RETAINED\n")
            from session_smoke import expect
            expect(peer,b'WB_OWNER_RETAINED')
            click(*data()['panes'][8]['primary_action']);wait_until(lambda:data()['terminal_action_open'])
            capture('lifecycle-transfer-confirm.png');click(*data()['confirm_terminal_action'])
            wait_until(lambda:data()['panes'][8]['ready'] and not data()['terminal_action_open']);revoked(peer)
        finally:peer.close()
        # Reconnect is non-stealing, read-only status never resurrects the service.
        peer=snapshot_attach(target_socket,takeover=True);collect(peer)
        try:
            wait_until(lambda:data()['panes'][8]['attachment']=='elsewhere')
            request(root,Path(runtime),root,'service.shutdown')
            click(*data()['panes'][8]['check_action']);wait_until(lambda:data()['panes'][8]['lifecycle'].get('state')=='unavailable' and not data()['panes'][8]['lifecycle'].get('busy'))
            assert not (root/'service.rpc').exists() and workspace_catalog.load(root)==life_catalog
            request(root,Path(runtime),root)  # Explicit test-owned service restart.
            click(*data()['panes'][8]['check_action']);wait_until(lambda:data()['panes'][8]['lifecycle'].get('state')=='running' and not data()['panes'][8]['lifecycle'].get('busy'))
            attempts=data()['panes'][8]['attachment_attempts']
            click(*settled_point(lambda d:None if d['panes'][8]['lifecycle'].get('busy') else d['panes'][8]['reconnect_action']));wait_until(lambda:data()['panes'][8]['attachment_attempts']>attempts and data()['panes'][8]['attachment']=='elsewhere')
            send(peer,16,b"printf WB_RECONNECT_DID_NOT_STEAL\n");expect(peer,b'WB_RECONNECT_DID_NOT_STEAL')
        finally:peer.close()
        time.sleep(.15);click(*data()['panes'][8]['reconnect_action']);wait_until(lambda:data()['panes'][8]['ready'] and data()['panes'][8]['focus'])
        after_reconnect=root/'after-reconnect-pid';typed('echo $$ > '+shlex.quote(str(after_reconnect)));key('Return');wait_until(after_reconnect.exists)
        assert after_reconnect.read_bytes()==recovered_pid.read_bytes()
        assert sizes()==metadata_sizes and {p['identity']:p['instance'] for p in data()['panes']}==life_ids
        # Cancel/Enter never closes a terminal; confirm removes one stable identity.
        def close_menu():
            click(*data()['panes'][8]['layout_button']);wait_until(lambda:data()['layout_menu_open'])
            click(*data()['terminal_close_item']);wait_until(lambda:data()['terminal_action_open'] and data()['terminal_action_kind']=='close')
            key('super+alt+Up');key('super+alt+t');time.sleep(.1)
            assert data()['selected']==8 and data()['workspace']==2 and len(data()['panes'])==14,'global actions escaped confirmation scope'
        close_menu();key('Return');time.sleep(.1)
        if data()['terminal_action_open']:key('Escape')
        wait_until(lambda:not data()['terminal_action_open'])
        assert workspace_catalog.load(root)==life_catalog and len(data()['panes'])==14
        close_menu();capture('lifecycle-close-confirm.png');click(*data()['confirm_terminal_action'])
        wait_until(lambda:not data()['terminal_action_open'] and len(data()['panes'])==13 and data()['selected']==-1)
        assert data()['workspace']==2 and not target_socket.exists()
        remaining={p['identity']:p['instance'] for p in data()['panes']}
        assert remaining=={identity:instance for identity,instance in life_ids.items() if identity!=target_identity},'closing replaced a neighbouring terminal item'
        assert all(p['ready'] for p in data()['panes'])
        for pid in pids:os.kill(int(pid),0)
        capture('lifecycle-empty-workspace.png')
        # Eight pane indicators and a long name must fit the minimum window width.
        key('super+alt+Up');wait_until(lambda:data()['workspace']==1)
        workspace_form('rename');key('ctrl+a');typed('A long workspace name that stays within the compact top bar');key('Return')
        wait_until(lambda:not data()['creation_open'])
        if wayland:run('niri','msg','action','set-window-width','680')
        else:cmd('windowsize','--sync',window,680,760)
        wait_until(lambda:data()['window_width']==680 and all(p['geometry_ready'] for p in data()['panes']))
        boxes=data()['chrome_boxes']
        assert boxes['workspace'][2] < boxes['indicators'][0] and boxes['indicators'][2] < boxes['count'][0]
        assert boxes['count'][2] <= boxes['add'][0] and boxes['add'][2] < boxes['overview'][0],'top-bar controls overlap'
        wait_until(lambda:display_data().get('window_logical_size',[0])[0]==data()['window_width'])
        capture('chrome-narrow.png')
        # Narrow Overview keeps readable two-column previews and stable PTY grids.
        overview_sizes=sizes();overview_ids=[p['instance'] for p in data()['panes']]
        key('super+alt+o');wait_until(lambda:data()['overview'] and data()['overview_columns']==2);time.sleep(.35)
        active=sorted((p for p in data()['panes'] if p['workspace']==1),key=lambda p:p['ordinal'])
        assert abs(active[0]['top']-active[1]['top'])<1 and active[2]['top']>active[1]['top']
        key('super+alt+Left');wait_until(lambda:data()['selected']!=11);time.sleep(.3)
        chosen=data()['panes'][data()['selected']]
        assert chosen['top']>=95 and chosen['bottom']+45<=748,'keyboard selection/caption remained outside viewport'
        capture('overview-narrow.png')
        assert sizes()==overview_sizes and [p['instance'] for p in data()['panes']]==overview_ids
        click(*data()['agent_filter_button']);wait_until(lambda:data()['agents_only'] and data()['overview_matches']==0)
        capture('overview-narrow-empty.png')
        click(*data()['overview_clear_filters']);wait_until(lambda:not data()['agents_only'] and not data()['needs_only'] and data()['overview_matches']==13)
        key('Escape');wait_until(lambda:not data()['overview'] and data()['panes'][data()['selected']]['focus'])
        font_menu();capture('font-narrow.png');key('Escape');wait_until(lambda:not data()['font_settings_open'])
        command_search('workspace');capture('commands-narrow.png')
        key('Escape');wait_until(lambda:not data()['commands_open'])
        stop_all_flow()
        # Retain a selected empty workspace when earlier rows are removed.
        survivor=request(root,Path(runtime),root,'workspace.create_at',title='Retained empty workspace',project_directory=str(root))['data']['workspaces'][-1]
        request(root,Path(runtime),root,'terminal.close',survivor['terminals'][0]['id'])
        wait_until(lambda:data()['workspace_count']==5 and len(data()['panes'])==13)
        assert not workspace_catalog.load(root)['workspaces'][-1]['terminals']
        while data()['workspace'] < data()['workspace_count']-1:
            previous_workspace=data()['workspace'];key('super+alt+Down');wait_until(lambda:data()['workspace']==previous_workspace+1)
        assert data()['selected']==-1
        # Removing the last workspace leaves a usable empty window, not seeded shells.
        while data()['workspace_count']:
            remaining=data()['workspace_count']
            click(*data()['workspaces_button']);wait_until(lambda:data()['drawer_open']);time.sleep(.3)
            click(*data()['remove_workspace_0']);wait_until(lambda:data()['terminal_action_open'] and not data()['drawer_open'])
            click(*data()['confirm_terminal_action']);wait_until(lambda:not data()['terminal_action_open'] and data()['workspace_count']==remaining-1)
            if remaining>1:assert data()['workspace_names'][data()['workspace']]==survivor['name'] and data()['selected']==-1
        assert not data()['panes'] and not data()['error'] and workspace_catalog.load(root)['workspaces']==[]
        capture('workspace-none.png')
        command_search('stop all terminals');assert not data()['command_matches'][0]['available'];key('Return');assert data()['commands_open'];key('Escape');wait_until(lambda:not data()['commands_open'])
        command_search('workspace appearance');assert not data()['command_matches'][0]['available'];key('Escape');wait_until(lambda:not data()['commands_open'])
        command_search('new workspace');key('Return');wait_until(lambda:data()['creation_open'])
        typed('Back again');key('Tab');key('Return');wait_until(lambda:data()['workspace_count']==1 and not data()['creation_open'] and len(data()['panes'])==1 and data()['panes'][0]['ready'])
        assert data()['workspace_names']==['Back again'] and not data()['error']
        capture('workspace-recreated.png')
        if wayland:run('niri','msg','action','close-window','--id',window)
        else:close_window(window,env)
        client.wait(timeout=5);assert client.returncode==0
        assert 'Qt UI: qrc:' not in (artifacts/'run.log').read_text(),'QML warning in UI study log'
        print(('wayland' if wayland else 'xcb')+' PASS: native shortcuts, per-pane width/full restore, workspace focus, live hidden output, Overview selection/Escape, drag/reorder/cancel/empty-workspace/full-destination/edge-scroll, click-without-selection, focus-aware cursor pixels/app shapes/hidden/glyph preservation, theme preview/cancel/apply/isolation/RGB+OSC preservation, directory .default discovery/preview/cancel/apply/remove/offline/read-only, creation/invalid-folder/cancel/empty-workspace, keyboard reorder/cross-workspace/empty/full/boundary/menu moves, rename/folder edits, standard folder picker/keyboard/cancel/Unicode URL roundtrip/cwd inheritance, real cwd/Git/worktree captions and agent lifecycle/attention filters, exit/recovery/reconnect/transfer/close with stable siblings, managed command/help/rename chords and old-chord passthrough, command menu/chord/filter/no-match/disabled/navigation/dialog/confirmation/Overview/minimum-width input isolation, font dotfiles/preview/cancel/apply/defaults/shortcuts with retained grids/objects/PIDs and managed bytes, unchanged reader PIDs and isolated input')
    except SystemExit:
        raise
    except BaseException:
        if report.exists():(artifacts/'failure.json').write_bytes(report.read_bytes())
        for evidence in root.glob('*.log'):(artifacts/('failure-'+evidence.name)).write_bytes(evidence.read_bytes())
        if client and client.poll() is None:
            try:capture('failure.png')
            except Exception:pass
        raise
    finally:
        if xdisplay:xlib.XCloseDisplay(xdisplay)
        for sock in root.glob('*.sock'):subprocess.run([runtime,'stop',str(sock)],capture_output=True,timeout=12)
        request(root,Path(runtime),root,"service.shutdown")
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
