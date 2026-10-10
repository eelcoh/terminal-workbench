#!/usr/bin/env python3
"""Shared catalog changes and explicit ownership transfer between real windows."""
from pathlib import Path
import json
import os
import select
import shutil
import socket
import threading
import subprocess
import sys
import tempfile
import time
from gui_smoke import close_window
from session_smoke import wait_until
ROOT=Path(__file__).resolve().parent.parent

def main():
    runtime,viewer=[str(Path(arg).resolve()) for arg in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='wb-windows-') as directory:
        root=Path(directory);state=root/'sessions';state.mkdir()
        data={'version':1,'workspaces':[
            {'id':'main','name':'Main','terminals':[{'id':'one','name':'One'},{'id':'two','name':'Two'}]},
            {'id':'scratch','name':'Scratch','terminals':[]}]}
        (state/'workspaces.json').write_text(json.dumps(data))
        processes=[]
        # Deterministically insert another creator's workspace after B's save
        # but before B receives its management result. A observes both first.
        wrapper=root/'catalog-wrapper.py'
        wrapper.write_text("import subprocess,sys,time\nfrom pathlib import Path\n"
            + "script="+repr(str(ROOT/'scripts/workspace_catalog.py'))+"\n"
            + "flag="+repr(str(root/'race'))+"\n"
            + "args=sys.argv[1:]\nresult=subprocess.run([sys.executable,script,*args],capture_output=True,text=True)\n"
            + "if result.returncode==0 and '--command' in args and args[args.index('--command')+1]=='workspace.create' and Path(flag).exists():\n"
            + "    concurrent=[('--name=Concurrent project' if a.startswith('--name=') else a) for a in args]\n"
            + "    subprocess.run([sys.executable,script,*concurrent],check=True,capture_output=True)\n"
            + "    Path(flag).unlink();time.sleep(.6)\n"
            + "sys.stdout.write(result.stdout);sys.stderr.write(result.stderr);raise SystemExit(result.returncode)\n")
        env=dict(os.environ,SHELL='/bin/sh',LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(state/'workspaces.tsv'),
            WORKBENCH_CATALOG_SCRIPT=str(ROOT/'scripts/workspace_catalog.py'),WORKBENCH_STATE_DIR=str(state),WORKBENCH_RUNTIME=runtime,WORKBENCH_PROJECT=str(root))
        env.pop('WAYLAND_DISPLAY',None);env.pop('WORKBENCH_SOCKET',None)
        with (root/'test.log').open('w+') as log:
            try:
                bootstrap=['python3',env['WORKBENCH_CATALOG_SCRIPT'],'--state-dir',str(state),'--runtime',runtime,'--cwd',str(root)]
                subprocess.run(bootstrap,env=env,check=True,stdout=log,stderr=log,timeout=15)
                read_fd,write_fd=os.pipe()
                xvfb=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(write_fd,),stdout=log,stderr=log)
                processes.append(xvfb);os.close(write_fd)
                assert select.select([read_fd],[],[],5)[0]
                env['DISPLAY']=':'+os.read(read_fd,32).decode().strip();os.close(read_fd)
                def cmd(*args):
                    result=subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,timeout=5)
                    result.check_returncode();return result.stdout.strip()
                def launch(environment=None):
                    process=subprocess.Popen([viewer],env=environment or env,cwd=root,stdout=log,stderr=log);processes.append(process)
                    def find():
                        assert process.poll() is None,process.returncode
                        result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True,timeout=5)
                        return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
                    window=wait_until(find,timeout=10);focus(window);time.sleep(.5)
                    return process,window
                def focus(window):
                    cmd('windowraise',window);cmd('windowfocus',window);time.sleep(.1)
                def key(window,value):
                    cmd('keydown','--window',window,value);time.sleep(.06);cmd('keyup','--window',window,value);time.sleep(.1)
                def click(window,x,y):
                    focus(window);cmd('mousemove','--window',window,x,y);cmd('mousedown',1);time.sleep(.06);cmd('mouseup',1);time.sleep(.15)
                def type_text(window,value):cmd('type','--clearmodifiers','--window',window,'--delay',12,'--',value)
                def line(window,value):focus(window);type_text(window,value);key(window,'Return')
                def read(name):return wait_until(lambda:(root/name).exists() and (root/name).read_text().strip(),timeout=10)
                def metadata():return json.loads((state/'workspaces.json').read_text())
                def edit(window,x,y,title,predicate):
                    click(window,x,y);type_text(window,title);key(window,'Return');wait_until(lambda:predicate(metadata()),timeout=10);time.sleep(.65)
                first,a=launch()
                line(a,'keep=original; printf "%s" "$$" > original.pid')
                pid=read('original.pid');inode=(state/'one.sock').stat().st_ino
                second,b=launch(dict(env,WORKBENCH_CATALOG_SCRIPT=str(wrapper)))
                key(b,'F12');time.sleep(.15)
                artifacts=ROOT/'.local/test-results';artifacts.mkdir(exist_ok=True)
                shutil.copyfile(sorted(root.glob('screenshot*.png'))[-1],artifacts/'move-here-header.png')
                click(b,535,58);time.sleep(.5)
                line(b,'printf "%s:%s" "$$" "$keep" > transferred')
                assert read('transferred')==pid+':original'
                # Transfer just one pane: its sibling remains usable in window A.
                click(a,900,250);line(a,'echo $$ > sibling.pid');sibling=read('sibling.pid')
                click(a,535,58);time.sleep(.5)
                line(a,'printf "%s:%s" "$$" "$keep" > returned')
                assert read('returned')==pid+':original'
                # GUI creation claims new terminals for the creating window even
                # if the other window observes the atomic catalog first.
                (root/'race').touch()
                edit(b,84,603,'Shared project',lambda d:len(d['workspaces'])==4)
                created=metadata()['workspaces'][2];created_id=created['id'];terminal=created['terminals'][0]['id']
                line(b,'keep=new; echo $$ > new.pid');new_pid=read('new.pid')
                click(a,84,238);line(a,'echo $$ > concurrent.pid')
                assert read('concurrent.pid')!=new_pid
                # Clicking the new row in A proves it appeared without relaunch.
                click(a,84,194);click(a,1050,58);time.sleep(.5)
                line(a,'printf "%s:%s" "$$" "$keep" > synchronized')
                assert read('synchronized')==new_pid+':new'
                # Saving an untouched rename dialog uses the refreshed name.
                edit(b,84,641,'Shared rename',lambda d:d['workspaces'][2]['name']=='Shared rename')
                click(a,84,641);key(a,'Return');time.sleep(.5)
                assert metadata()['workspaces'][2]['name']=='Shared rename'
                edit(b,1180,58,'Shared terminal',lambda d:d['workspaces'][2]['terminals'][0]['name']=='Shared terminal')
                click(a,1180,58);key(a,'Return');time.sleep(.5)
                assert metadata()['workspaces'][2]['terminals'][0]['name']=='Shared terminal'
                # New terminal creation also synchronizes while keeping local focus.
                click(b,84,679);key(b,'Return')
                wait_until(lambda:len(metadata()['workspaces'][2]['terminals'])==2,timeout=10);time.sleep(.65)
                extra=metadata()['workspaces'][2]['terminals'][1]['id']
                line(b,'keep=extra; echo $$ > extra.pid');extra_pid=read('extra.pid')
                line(a,'printf "%s:%s" "$$" "$keep" > focus-retained')
                assert read('focus-retained')==new_pid+':new'
                click(a,1050,58);time.sleep(.5)
                line(a,'printf "%s:%s" "$$" "$keep" > extra-synchronized')
                assert read('extra-synchronized')==extra_pid+':extra'
                subprocess.run(bootstrap+['--command','terminal.move','--id',extra,'--to','main'],env=env,check=True,stdout=log,stderr=log,timeout=10);time.sleep(.65)
                # A focused terminal moved elsewhere: focus falls back safely;
                # the other window's workspace selection remains independent.
                line(a,'printf "%s:%s" "$$" "$keep" > move-sync')
                assert read('move-sync')==new_pid+':new'
                subprocess.run(bootstrap+['--command','workspace.close','--id',created_id],env=env,check=True,stdout=log,stderr=log,timeout=20);time.sleep(.65)
                # Both windows select a surviving workspace after removing their active one.
                # Selecting Main again retains its original live connections.
                click(a,84,106);click(a,300,250)
                line(a,'printf "%s:%s" "$$" "$keep" > survived-close')
                assert read('survived-close')==pid+':original'
                click(b,84,106);click(b,535,58);time.sleep(.5)
                line(b,'printf "%s:%s" "$$" "$keep" > close-sync')
                assert read('close-sync')==pid+':original'
                assert (state/'one.sock').stat().st_ino==inode
                os.kill(int(pid),0);os.kill(int(sibling),0);os.kill(int(extra_pid),0)
                focus(a);close_window(a,env);assert first.wait(timeout=5)==0
                # Emulate a pre-transfer .ctl server while preserving its running
                # shell: unknown ATM1 closes; ordinary methods are forwarded.
                alias=root/'old.sock';alias.symlink_to(state/'one.sock')
                listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
                listener.bind(str(alias.with_suffix('.ctl')));listener.listen()
                def proxy(connection):
                    with connection:
                        upstream=None
                        try:
                            connection.settimeout(1)
                            method=connection.recv(5,socket.MSG_WAITALL)
                            if method==b'ATM1\n':return
                            upstream=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
                            upstream.connect(str(state/'one.ctl'));upstream.sendall(method)
                            connection.settimeout(None)
                            while True:
                                ready,_,_=select.select([connection,upstream],[],[],2)
                                for source in ready:
                                    value=source.recv(65536)
                                    if not value:return
                                    (upstream if source is connection else connection).sendall(value)
                        except OSError:pass
                        finally:
                            if upstream:upstream.close()
                def accept():
                    while True:
                        try:connection,_=listener.accept()
                        except OSError:return
                        threading.Thread(target=proxy,args=(connection,),daemon=True).start()
                threading.Thread(target=accept,daemon=True).start()
                older_catalog=root/'old.tsv'
                older_catalog.write_text(f'workbench-catalog-v3\nW\told\tEarlier runtime\nT\told\tone\tOne\t{alias}\n')
                older,c=launch(dict(env,WORKBENCH_WORKSPACES=str(older_catalog)))
                click(c,1050,58);time.sleep(.6)
                log.flush();assert 'Earlier runtime: retrying ordinary attachment' in (root/'test.log').read_text()
                line(b,'printf "%s:%s" "$$" "$keep" > old-owner-preserved')
                assert read('old-owner-preserved')==pid+':original'
                focus(b);close_window(b,env);assert second.wait(timeout=5)==0;time.sleep(.3)
                click(c,1050,58);time.sleep(.6)
                line(c,'printf "%s:%s" "$$" "$keep" > old-reconnect')
                assert read('old-reconnect')==pid+':original'
                focus(c);close_window(c,env);assert older.wait(timeout=5)==0
                listener.close()
            finally:
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                        try:process.wait(timeout=5)
                        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
                for endpoint in state.glob('*.sock'):
                    subprocess.run([runtime,'stop',str(endpoint)],capture_output=True,timeout=15)
                if sys.exc_info()[0]:log.flush();print((root/'test.log').read_text(),file=sys.stderr)
    print('PASS: two-window bidirectional transfer, same PID/state/socket, sibling ownership, shared create/rename/move/close, creator ownership under overlapping saves, independent focus, live survivors and safe earlier-runtime reconnect')
if __name__=='__main__':main()
