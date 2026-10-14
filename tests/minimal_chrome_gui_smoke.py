#!/usr/bin/env python3
"""Native chrome scopes, drawer geometry, stable menu targets and window ownership."""
from gui_smoke import close_window
from pathlib import Path
import fcntl,json,os,select,shutil,struct,subprocess,sys,tempfile,termios,time,uuid
from session_smoke import wait_until
from session_service_test import request
ROOT=Path(__file__).resolve().parent.parent

def main():
    runtime,viewer=[Path(x).resolve() for x in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-chrome-') as temporary:
        root=Path(temporary);state=root/'sessions';processes=[];os.environ['SHELL']='/bin/sh'
        def call(method='initialize',identity='',title='',**kwargs):return request(state,runtime,root,method,identity,title,**kwargs)
        def data():return json.loads((state/'workspaces.json').read_text())
        with (root/'test.log').open('w+') as log:
            try:
                call();initial=data();first=initial['workspaces'][0]['terminals'][0]['id']
                rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
                assert select.select([rd],[],[],5)[0]
                with os.fdopen(rd) as pipe:display=pipe.readline().strip()
                env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root),WORKBENCH_SHORTCUTS=str(root/'shortcuts.conf'),WORKBENCH_WINDOW_STATE=str(root/'window.view'))
                env.pop('WAYLAND_DISPLAY',None);env.pop('WORKBENCH_SOCKET',None)
                def cmd(*args):
                    r=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5);r.check_returncode();return r.stdout.strip()
                wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
                def launch(view='window.view'):
                    e=dict(env,WORKBENCH_WINDOW_STATE=str(root/view));p=subprocess.Popen([str(viewer)],cwd=root,env=e,stdout=log,stderr=log);processes.append(p)
                    def find():
                        assert p.poll() is None
                        r=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(p.pid)],env=e,capture_output=True,text=True)
                        return r.stdout.strip().splitlines()[-1] if r.returncode==0 else None
                    w=wait_until(find);cmd('windowfocus',w);time.sleep(.5);return p,w
                process,window=launch()
                def key(value):cmd('keydown','--window',window,value);time.sleep(.08);cmd('keyup','--window',window,value);time.sleep(.08)
                def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',12,'--',value)
                def line(value):text(value);key('Return')
                def click(x,y):cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.06);cmd('mouseup',1);time.sleep(.15)
                def marker(name):return wait_until(lambda:(root/name).exists() and (root/name).read_text().strip())
                def screenshot(name):
                    started=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=started),None))
                    dest=ROOT/'.local/test-results'/('minimal-'+name+'.png');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,dest);key("ctrl+u");return dest
                def app_menu(index):click(22,20);click(100,65+index*32+8*sum(index>=boundary for boundary in [2,5,10,12]))
                def palette(query):key('ctrl+shift+p');text(query);key('Return')
                def focus_first():key('ctrl+shift+Up');key('ctrl+shift+Left');time.sleep(.25)
                line('keep=chrome; echo $$ > original.pid');pid=marker('original.pid');inode=(state/f'{first}.sock').stat().st_ino
                def geometry():
                    fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NOCTTY)
                    try:return fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                    finally:os.close(fd)
                baseline=geometry();assert struct.unpack('HHHH',baseline)[:2]==(41,82)
                def safe(name):line(f'printf "%s:%s" "$$" "$keep" > {name}');assert marker(name)==pid+':chrome';assert (state/f'{first}.sock').stat().st_ino==inode
                screenshot('working-dark')
                # Drawer opening and dismissal consume text and pointer events but never resize PTYs.
                click(22,66);text('touch_sentinel');assert geometry()==baseline
                key('Escape');safe('drawer-safe')
                click(22,20);screenshot('app-menu-sections');text('menu_sentinel');key('Escape');safe('menu-safe')
                click(22,66);click(235,65);time.sleep(.3);assert geometry()!=baseline
                wait_until(lambda:'C 1 0 0' in (root/'window.view').read_text());screenshot('pinned')
                click(235,65);time.sleep(.3);assert geometry()==baseline;safe('pin-safe')
                # Explicit menu action reuses rename dialog and does not fall through into it.
                app_menu(3);key('ctrl+a');text('Workbench project');key('Return');wait_until(lambda:data()['workspaces'][0]['name']=='Workbench project')
                safe('rename-safe')
                click(629,62);screenshot('pane-menu');key('Return');key('ctrl+a');text('Implementation');key('Return')
                wait_until(lambda:data()['workspaces'][0]['terminals'][0]['name']=='Implementation');safe('pane-rename-safe')
                # Command registry presets remain mouse and keyboard accessible.
                click(629,62);click(725,103+4*32+8);time.sleep(.25);assert geometry()!=baseline;palette('Terminal width: one half');time.sleep(.25);assert geometry()==baseline
                key('ctrl+shift+r');assert geometry()!=baseline;key('ctrl+shift+r');key('ctrl+shift+r');time.sleep(.2);assert geometry()==baseline
                app_menu(0);text('Menu-created');key('Return');wait_until(lambda:len(data()['workspaces'][0]['terminals'])==3)
                time.sleep(.5);line('echo $$ > disposable.pid');disposable=marker('disposable.pid')
                palette('Stop terminal');key('Return');os.kill(int(disposable),0);key('Tab');key('Return');os.kill(int(disposable),0)
                palette('Stop terminal');key('Tab');key('Tab');key('Return');wait_until(lambda:len(data()['workspaces'][0]['terminals'])==2)
                focus_first();safe('stop-safe')
                # Agent activity is runtime-derived; named-process fixture verifies projection only.
                fixture=root/'codex';shutil.copyfile('/usr/bin/sleep',fixture);fixture.chmod(0o700)
                key('ctrl+shift+Down');line(f"sh -c 'echo $$ > agent.pid; exec {fixture} 120'")
                agent_id=data()['workspaces'][1]['terminals'][0]['id'];sock=state/f'{agent_id}.sock'
                def inspect():return json.loads(subprocess.run([str(runtime),'inspect',str(sock)],capture_output=True,text=True,check=True).stdout)
                wait_until(lambda:inspect()['agent']=='codex')
                hook=dict(env,WORKBENCH_SESSION_SOCKET=str(sock),WORKBENCH_AGENT_GROUP=str(os.getpgid(int(marker('agent.pid')))),WORKBENCH_AGENT_RUN=uuid.uuid4().hex)
                def report(event,status):
                    subprocess.run([sys.executable,str(ROOT/'scripts/agent-hook.py'),'codex'],input=json.dumps({'hook_event_name':event,'notification_type':'permission_prompt'}),text=True,env=hook,check=True,capture_output=True)
                    wait_until(lambda:inspect()['status']==status);time.sleep(.35)
                report('PermissionRequest','waiting');key('ctrl+shift+Up');click(22,108);click(210,112);screenshot('agents-attention');click(120,160)
                key('ctrl+c');wait_until(lambda:inspect()['agent'] is None)
                key('ctrl+shift+Up');safe('agent-jump-safe')
                # Menus bind to stable pane identity across catalog refresh and reject removed targets.
                click(629,62);call('terminal.rename',first,'Renamed outside');time.sleep(.5);key('Return');key('Escape');safe('refresh-menu-safe')
                # A real pane menu moves the sibling through the same catalog workflow.
                sibling=data()['workspaces'][0]['terminals'][1]['id'];main_id=data()['workspaces'][0]['id'];scratch_id=data()['workspaces'][1]['id']
                click(1210,62);click(1040,103+6*32+16);click(1040,135)
                wait_until(lambda:len(data()['workspaces'][0]['terminals'])==1)
                assert data()['workspaces'][1]['terminals'][-1]['id']==sibling
                call('terminal.move',sibling,destination=main_id,position=1)
                wait_until(lambda:len(data()['workspaces'][0]['terminals'])==2);time.sleep(.5);focus_first();safe('menu-move-safe')
                # Switch to another workspace through a keyboard-operated drawer.
                click(22,66);key('Tab');key('Tab');key('Tab');key('Return');line('echo $$ > drawer-keyboard.pid');marker('drawer-keyboard.pid')
                focus_first();safe('drawer-keyboard-safe')
                # Pin and mode persist per window. Temporary drawer/menu never persist as open.
                click(22,108);click(235,65);wait_until(lambda:'C 1 1 1' in (root/'window.view').read_text())
                close_window(window,env);assert process.wait(timeout=5)==0
                process,window=launch();time.sleep(.3);assert geometry()!=baseline;screenshot('restored-agents')
                click(235,65);time.sleep(.3);assert geometry()==baseline;safe('restored-safe')
                # Ownership remains available directly in the quiet pane header.
                first_window=window;owner=process;second,window=launch('second.view');screenshot('elsewhere')
                click(556,62);time.sleep(.7);line('printf "%s:%s" "$$" "$keep" > transferred.pid');assert marker('transferred.pid')==pid+':chrome'
                close_window(window,env);assert second.wait(timeout=5)==0
                window=first_window;cmd('windowfocus',window);click(556,62);time.sleep(.7);safe('transfer-back-safe')
                # Cross on another workspace never navigates to it or stops shells before confirmation.
                scratch_pid=marker('drawer-keyboard.pid')
                click(22,66);cmd('mousemove','--window',window,259,169);screenshot('workspace-close-hover');click(259,169);screenshot('workspace-close-confirm')
                key('Return');assert len(data()['workspaces'])==2;os.kill(int(scratch_pid),0)
                key('Tab');key('Return');safe('workspace-close-cancel');assert geometry()==baseline
                # Keyboard reaches close affordances after the navigation rows; Escape is safe.
                click(22,66)
                for _ in range(5):key('Tab')
                key('Return');key('Escape');safe('keyboard-workspace-close-cancel')
                click(22,66);click(259,169)
                call('workspace.rename',scratch_id,'Renamed during confirmation')
                key('Tab');key('Tab');key('Return');wait_until(lambda:len(data()['workspaces'])==1)
                assert data()['workspaces'][0]['id']==main_id
                # Shared metadata commits before the window consumes the save reply.
                # Wait for its own persisted catalog before typing into the shell.
                def window_removed_workspace():
                    view=root/'window.view'
                    if not view.exists():return False
                    identities={row.split()[1] for row in view.read_text().splitlines() if row.startswith('W ')}
                    return identities=={main_id}
                wait_until(window_removed_workspace)
                safe('workspace-close-target-safe')
                wait_until(lambda:not Path(f'/proc/{scratch_pid}').exists())
                # Empty workspace removal uses the same confirmation, with zero affected shells.
                call('workspace.create',title='Empty removal');empty=data()['workspaces'][-1]
                for terminal in empty['terminals']:call('terminal.close',terminal['id'])
                time.sleep(.5);click(22,66);click(259,169);screenshot('empty-workspace-close')
                key('Tab');key('Tab');key('Return');wait_until(lambda:len(data()['workspaces'])==1);safe('empty-close-safe')
                # A move menu with no available destinations still opens and dismisses normally.
                click(629,62);click(725,103+6*32+16);key('Down');key('Return');key('Escape');safe('disabled-menu-safe')
                # Empty workspace uses only the application background and the visible + action.
                palette('Stop workspace');key('Escape')
                app_menu(1);text('Empty candidate');key('Escape');safe('create-cancel-safe')
                palette('Change appearance');text('Dawn');key('shift+Tab');key('space');time.sleep(.3);screenshot('working-light')
                cmd('windowsize',window,800,300);time.sleep(.3);click(22,20);screenshot('small-menu');key('Escape');cmd('windowsize',window,1240,760);time.sleep(.3);safe('small-safe')
                click(22,20);key('Up');cmd('keydown','--window',window,'Return');assert owner.wait(timeout=5)==0;cmd('keyup','Return');os.kill(int(pid),0)
                print('PASS: native menus, input isolation, drawer/pin geometry and persistence, rename/create/confirmed stop, agent jump, stable menu target refresh, ownership transfer/back, light/dark/narrow chrome and same live shell')
            finally:
                if sys.exc_info()[0]:
                    log.flush();print((root/'test.log').read_text()[-2500:],file=sys.stderr)
                    for sock in state.glob('*.sock'):
                        print(sock.name,subprocess.run([str(runtime),'read',str(sock)],capture_output=True,text=True).stdout[-2000:],file=sys.stderr)
                for p in reversed(processes):
                    if p.poll() is None:p.terminate();p.wait(timeout=5)
                for sock in state.glob('*.sock'):subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=15)
                call('service.shutdown')
if __name__=='__main__':main()
