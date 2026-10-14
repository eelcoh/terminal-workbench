#!/usr/bin/env python3
"""Real name/folder fields: partial editing, clipboard, pointer selection, isolation."""
from pathlib import Path
import fcntl,json,os,select,shlex,shutil,subprocess,sys,tempfile,termios,time
from gui_smoke import close_window
from session_smoke import attach,send,wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request

runtime,viewer=[Path(x).resolve() for x in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='wb-edit-') as temporary:
    root=Path(temporary);state=root/'sessions';processes=[];os.environ['SHELL']='/bin/sh'
    with (root/'test.log').open('w+') as log:
        try:
            def call(method='initialize',identity='',title='',**kw):return request(state,runtime,root,method,identity,title,**kw)['data']
            data=call();main=data['workspaces'][0]['id'];first=data['workspaces'][0]['terminals'][0]['id'];sock=state/(first+'.sock')
            call('workspace.rename',main,'alpha beta')
            base=root/('folder-'+'a'*100)/'Unicode λ';old=base/'leaf_one';new=base/'leaf_two';old.mkdir(parents=True);new.mkdir()
            call('workspace.directory',main,project_directory=str(old))
            connection,_,_=attach(sock);send(connection,16,('keep=editor;echo $$ > '+shlex.quote(str(root/'pid'))+'\n').encode());wait_until((root/'pid').exists);pid=(root/'pid').read_text().strip();connection.close()
            rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as ready:display=ready.readline().strip()
            env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_RUNTIME=str(runtime),WORKBENCH_PROJECT=str(root),WORKBENCH_WINDOW_STATE=str(root/'window.view'))
            env.pop('WAYLAND_DISPLAY',None);env.pop('WORKBENCH_SOCKET',None)
            wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
            process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
            def cmd(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            def find():
                assert process.poll() is None
                result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
            window=wait_until(find,timeout=10);cmd('windowfocus',window);time.sleep(.5)
            def key(value):cmd('keydown','--window',window,value);time.sleep(.06);cmd('keyup','--window',window,value);time.sleep(.08)
            def text(value):cmd('type','--clearmodifiers','--window',window,'--delay',20,'--',value)
            def clipboard():return subprocess.check_output(['xclip','-selection','clipboard','-o'],env=env,timeout=5).decode()
            def put(value):subprocess.run(['xclip','-selection','clipboard','-i'],input=value.encode(),env=env,stdout=subprocess.DEVNULL,stderr=log,check=True,timeout=5)
            def paste(value):put(value);key('ctrl+v')
            def metadata():return json.loads((state/'workspaces.json').read_text())
            def saved(predicate):wait_until(lambda:predicate(metadata()));time.sleep(.4)
            def palette(value):key('ctrl+shift+p');text(value);key('Return');time.sleep(.2)
            def geometry():
                fd=os.open(f'/proc/{pid}/fd/0',os.O_RDONLY|os.O_NONBLOCK)
                try:return fcntl.ioctl(fd,termios.TIOCGWINSZ,bytes(8))
                finally:os.close(fd)
            baseline=geometry()
            def capture(name):
                start=time.time_ns();key('F12');image=wait_until(lambda:next((p for p in root.glob('screenshot*.png') if p.stat().st_mtime_ns>=start),None))
                target=ROOT/'.local/artifacts/text-edit'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy(image,target)
            def click(x,y):cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.06);cmd('mouseup',1);time.sleep(.1)
            def safe(name):
                text('printf "%s:%s" "$$" "$keep" > '+shlex.quote(str(root/name)));key('Return')
                assert wait_until(lambda:(root/name).exists() and (root/name).read_text())==pid+':editor'
                assert geometry()==baseline
            palette('Rename workspace');key('Home')
            for _ in range(5):key('Right')
            key('shift+End');key('ctrl+c');assert clipboard()==' beta';capture('name-selection.png')
            key('ctrl+x');text(' project');key('Home');key('ctrl+Right');key('ctrl+v')
            key('ctrl+BackSpace');key('BackSpace');key('Return');saved(lambda d:d['workspaces'][0]['name']=='alpha project');safe('partial-name-safe')
            palette('Rename workspace');key('ctrl+a');paste('naïve λ');key('End');key('BackSpace');paste('β');key('Left');key('shift+Left');text('-');key('Return')
            saved(lambda d:d['workspaces'][0]['name']=='naïve-β');safe('unicode-name-safe')
            palette('Rename workspace');key('ctrl+a');paste('abcdef');click(443,357);key('Right');key('Right');text('X');key('ctrl+a');key('ctrl+c');assert clipboard()=='abXcdef'
            cmd('mousemove','--window',window,798,357);cmd('mousedown',1);time.sleep(.08);cmd('mousemove','--window',window,443,357);time.sleep(.1);cmd('mouseup',1);time.sleep(.1)
            key('ctrl+c');assert clipboard()=='abXcdef';text('alpha beta');time.sleep(.4)
            cmd('mousemove','--window',window,443,357);cmd('click','--repeat',2,'--delay',100,1);time.sleep(.1);key('ctrl+c');assert clipboard()=='alpha'
            capture('mouse-word-selection.png');time.sleep(.4);cmd('click','--repeat',3,'--delay',100,1);time.sleep(.1);key('ctrl+c');assert clipboard()=='alpha beta'
            text('mouse edit');key('Return');saved(lambda d:d['workspaces'][0]['name']=='mouse edit');safe('mouse-edit-safe')
            palette('Change project folder');key('ctrl+a');key('ctrl+c');assert clipboard()==str(old)
            key('End');key('ctrl+shift+Left');key('ctrl+c');assert clipboard()=='leaf_one';paste('leaf_two');capture('folder-end-caret.png')
            key('Home')
            for _ in range(5):key('Right')
            key('ctrl+shift+End');key('ctrl+c');assert clipboard()==str(new)[5:];capture('folder-partial-selection.png')
            key('Return');saved(lambda d:d['workspaces'][0]['directory']==str(new));safe('folder-edit-safe')
            text('pwd > '+shlex.quote(str(root/'cwd')));key('Return');assert wait_until(lambda:(root/'cwd').exists() and (root/'cwd').read_text().strip())==str(root)
            before=(state/'workspaces.json').read_bytes();palette('Rename workspace');key('ctrl+a');paste('bad\nvalue');key('ctrl+c');assert clipboard()=='mouse edit'
            paste('x'*100);key('ctrl+c');assert clipboard()=='mouse edit';capture('rejected-paste.png');text('unsaved');key('Escape')
            assert (state/'workspaces.json').read_bytes()==before;safe('cancel-safe')
            key('ctrl+shift+n');text('Draft');key('End');key('Left');key('Tab');key('ctrl+a');paste(str(old));key('Home');key('shift+Tab');text('X');key('ctrl+a');key('ctrl+c');assert clipboard()=='DrafXt'
            key('Tab');key('ctrl+a');key('ctrl+c');assert clipboard()==str(old);key('Tab');key('Tab');key('space');assert (state/'workspaces.json').read_bytes()==before;safe('dual-fields-safe')
            palette('Rename terminal');key('End');text(' debug');key('Return');saved(lambda d:d['workspaces'][0]['terminals'][0]['name']=='Shell 1 debug');safe('terminal-rename-safe')
            close_window(window,env);assert process.wait(timeout=5)==0
            print('PASS: partial/Unicode edits, word navigation, clipboard copy/cut/paste, mouse caret/drag/double/triple clicks, long path scrolling, per-field caret preservation, rejected paste/cancel isolation and unchanged live shell/grid/cwd')
        except Exception:
            log.flush();print((root/'test.log').read_text()[-5000:]);print('Failure metadata:',metadata());raise
        finally:
            for p in reversed(processes):
                if p.poll() is None:p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
            for sock in state.glob('*.sock'):subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=10)
            subprocess.run(['python3',str(ROOT/'scripts/session_service.py'),'stop','--state-dir',str(state)],capture_output=True,timeout=10)
