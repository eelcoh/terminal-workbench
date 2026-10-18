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
        managed=root/'config.conf';managed.write_text('workbench-config-v1\n# managed fixture\n[appearance]\napplication-theme = missing-application-theme\nterminal-theme = missing-terminal-theme\n[macos.font]\nsize = 20\n[font]\nfamily = Fira Code\nsize = 14\n[linux.font]\nsize = 16\n[shortcuts]\ncommands = F10\nshortcuts.help = F9\nworkspace.rename = F8\n');managed_bytes=managed.read_bytes()
        config=root/'sessions.json';config.write_text(json.dumps({'state':str(root),'runtime':runtime,'cwd':str(root),'catalog':catalog,'theme_store':str(theme_store),'preferences':str(managed),'shortcut_overrides':str(root/'ui/linux/shortcuts.conf'),'theme_directory':str(root/'themes')}))
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
        def drag_workspace(slot,workspace):
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
                wait_until(lambda:data()['drop_space']==workspace and data()['drop_allowed'])
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
        typed('first');wait_until(lambda:received(0)==b'first')
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
        # Palette matching/input is local: no shell, geometry or ownership changes.
        command_grid=sizes();command_pids=[p['instance'] for p in data()['panes']]
        command_input=[received(i) for i in range(6)]
        def command_search(query):
            if not data()['commands_open']:
                key('super+alt+p');wait_until(lambda:data()['commands_open'] and data()['command_search_focus'])
            key('ctrl+a');typed(query)
            wait_until(lambda:bool(data()['command_matches']) and all(all(word in (entry['title']+' '+entry['detail']).lower() for word in query.lower().split()) for entry in data()['command_matches']))
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
        command_search('bundled font');key('Return');wait_until(lambda:data()['font_settings_open'] and not data()['commands_open'])
        click(*data()['font_jetbrains']);click(*data()['font_plus']);wait_until(lambda:settled_font('JetBrains Mono',17))
        click(*data()['font_apply']);wait_until(lambda:not data()['font_settings_open'] and settled_font('JetBrains Mono',17))
        font_menu();click(*data()['font_defaults_button']);wait_until(lambda:settled_font('Fira Code',16))
        key('Escape');wait_until(lambda:not data()['font_settings_open'] and settled_font('JetBrains Mono',17))
        key('super+alt+0');wait_until(lambda:settled_font('JetBrains Mono',16))
        key('super+alt+equal');wait_until(lambda:settled_font('JetBrains Mono',17))
        key('super+alt+minus');wait_until(lambda:settled_font('JetBrains Mono',16))
        font_menu();click(*data()['font_defaults_button']);wait_until(lambda:settled_font('Fira Code',16))
        click(*data()['font_apply']);wait_until(lambda:not data()['font_settings_open'] and sizes()==font_grid and data()['panes'][0]['focus'])
        assert [p['instance'] for p in data()['panes']]==font_items and [received(i) for i in range(6)]==font_input
        assert managed.read_bytes()==managed_bytes,'font UI rewrote managed settings'
        for i,pid in enumerate(pids):assert (root/str(i)/'pid').read_text()==pid;os.kill(int(pid),0)
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
        drop_file.unlink();appearance();wait_until(lambda:data()['theme_catalog_count']==7)
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
        pane=data()['panes'][0];target=data()['panes'][3]
        move(pane['x'],pane['y']);button('mousedown')
        try:
            move(target['x'],target['y']);wait_until(lambda:data()['drag_slot']==0 and data()['drop_space']==1 and not data()['drop_allowed'])
            capture('drag-full-workspace.png')
        finally:button('mouseup')
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
        click(*data()['agents_button']);wait_until(lambda:data()['overview'] and data()['agents_only'] and data()['overview_matches']==1)
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
            click(*data()['panes'][8]['reconnect_action']);wait_until(lambda:data()['panes'][8]['attachment_attempts']>attempts and data()['panes'][8]['attachment']=='elsewhere')
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
        capture('chrome-narrow.png')
        font_menu();capture('font-narrow.png');key('Escape');wait_until(lambda:not data()['font_settings_open'])
        command_search('workspace');capture('commands-narrow.png')
        key('Escape');wait_until(lambda:not data()['commands_open'])
        if wayland:run('niri','msg','action','close-window','--id',window)
        else:close_window(window,env)
        client.wait(timeout=5);assert client.returncode==0
        assert 'Qt UI: qrc:' not in (artifacts/'run.log').read_text(),'QML warning in UI study log'
        print(('wayland' if wayland else 'xcb')+' PASS: native shortcuts, per-pane width/full restore, workspace focus, live hidden output, Overview selection/Escape, drag/reorder/cancel/empty-workspace/full-destination/edge-scroll, click-without-selection, focus-aware cursor pixels/app shapes/hidden/glyph preservation, theme preview/cancel/apply/isolation/RGB+OSC preservation, directory .default discovery/preview/cancel/apply/remove/offline/read-only, creation/invalid-folder/cancel/empty-workspace, keyboard reorder/cross-workspace/empty/full/boundary/menu moves, rename/folder edits, standard folder picker/keyboard/cancel/Unicode URL roundtrip/cwd inheritance, real cwd/Git/worktree captions and agent lifecycle/attention filters, exit/recovery/reconnect/transfer/close with stable siblings, managed command/help/rename chords and old-chord passthrough, command menu/chord/filter/no-match/disabled/navigation/dialog/confirmation/Overview/minimum-width input isolation, font dotfiles/preview/cancel/apply/defaults/shortcuts with retained grids/objects/PIDs and managed bytes, unchanged reader PIDs and isolated input')
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
