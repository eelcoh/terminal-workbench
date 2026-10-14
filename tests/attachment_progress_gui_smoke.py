#!/usr/bin/env python3
"""Real viewer with slow snapshot transports, navigation and READY input guards."""
from pathlib import Path
import json,os,select,shutil,socket,struct,subprocess,sys,tempfile,threading,time
from session_smoke import wait_until,read_frame
from snapshot_attach_smoke import snapshot_attach,collect
from gui_smoke import close_window
ROOT=Path(__file__).resolve().parent.parent
runtime,viewer=[Path(value).resolve() for value in sys.argv[1:3]]
count=int(os.environ.get('WORKBENCH_TEST_PANES','1'))
def frame(stream,kind,payload=b''):stream.sendall(struct.pack('!BI',kind,len(payload))+payload)
with tempfile.TemporaryDirectory(prefix='wb-attach-ui-') as temporary:
    root=Path(temporary).resolve();processes=[];listeners=[];connections=[];errors=[];ready=set();release=threading.Event();stop=threading.Event()
    with (root/'gui.log').open('w+') as log:
        try:
            helper=root/'fixture';helper.write_text('#!'+sys.executable+'\n'+"import os,sys,tty\nfrom pathlib import Path\ntty.setraw(0)\nidentity=sys.argv[0].rsplit('/',1)[-1]\nfor row in range(900):os.write(1,('row-'+str(row)+' history '+('x'*60)+'\\r\\n').encode())\nos.write(1,b'\\x1b[2J\\x1b[HRESTORED_TERMINAL')\nPath(identity+'.ready').touch()\nwhile True:\n data=os.read(0,4096)\n with Path(identity+'.input').open('ab') as file:file.write(data)\n")
            helper.chmod(0o700)
            sources=[];rows=['workbench-catalog-v3'];started=time.monotonic()
            for index in range(count):
                source=root/('r'+str(index)+'.sock');sources.append(source)
                workload=root/('f'+str(index));shutil.copyfile(helper,workload);workload.chmod(0o700)
                process=subprocess.Popen([str(runtime),str(source),str(workload)],cwd=root,stdout=log,stderr=log);processes.append(process)
                wait_until(lambda:Path(str(workload)+'.ready').exists())
                path=root/('p'+str(index)+'.sock');listener=socket.socket(socket.AF_UNIX);listener.bind(str(path.with_suffix('.ctl')));listener.listen(32);listeners.append(listener)
                def accept(listener=listener,source=source,index=index):
                    def handle(peer):
                        backend=None
                        try:
                            method=peer.recv(5)
                            if method!=b'ATS1\n':return
                            revision=b''
                            while len(revision)<40:revision+=peer.recv(40-len(revision))
                            backend=snapshot_attach(source);connections.append(backend);snapshot=collect(backend)
                            frame(peer,7,len(snapshot).to_bytes(4,'big'))
                            half=len(snapshot)//2
                            for position in range(0,half,16384):frame(peer,8,snapshot[position:min(position+16384,half)])
                            assert release.wait(15),'GUI did not release slow transfer'
                            for position in range(half,len(snapshot),16384):frame(peer,8,snapshot[position:position+16384]);time.sleep(.005)
                            frame(peer,9);frame(peer,3);ready.add(index)
                            def upstream():
                                try:
                                    while not stop.is_set():
                                        data=peer.recv(65536)
                                        if not data:return
                                        backend.sendall(data)
                                except OSError:pass
                            threading.Thread(target=upstream,daemon=True).start()
                            while not stop.is_set():
                                data=backend.recv(65536)
                                if not data:return
                                peer.sendall(data)
                        except (OSError,EOFError) as error:
                            if not stop.is_set() and index not in ready:errors.append(str(error))
                        except AssertionError as error:errors.append(str(error))
                        finally:
                            if backend:backend.close()
                            peer.close()
                    while not stop.is_set():
                        try:peer,_=listener.accept()
                        except OSError:return
                        connections.append(peer);threading.Thread(target=handle,args=(peer,),daemon=True).start()
                threading.Thread(target=accept,daemon=True).start()
                if index%8==0:rows.append('W\tw'+str(index//8)+'\tWorkspace '+str(index//8))
                rows.append('T\tw'+str(index//8)+'\tp'+str(index)+'\tPane '+str(index)+'\t'+str(path))
            catalog=root/'workspaces.tsv';catalog.write_text('\n'.join(rows)+'\n')
            rd,wr=os.pipe();xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1440x1000x24','-noreset','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
            assert select.select([rd],[],[],5)[0]
            with os.fdopen(rd) as pipe:display=pipe.readline().strip()
            env=dict(os.environ,DISPLAY=':'+display,LIBGL_ALWAYS_SOFTWARE='1',WORKBENCH_WORKSPACES=str(catalog),WORKBENCH_REDUCED_MOTION='1')
            for name in ('WAYLAND_DISPLAY','WORKBENCH_SOCKET','WORKBENCH_CONFIG','WORKBENCH_CATALOG_SCRIPT','WORKBENCH_STATE_DIR'):env.pop(name,None)
            wait_until(lambda:subprocess.run(['xdotool','getdisplaygeometry'],env=env,capture_output=True).returncode==0)
            process=subprocess.Popen([str(viewer)],cwd=root,env=env,stdout=log,stderr=log);processes.append(process)
            def command(*args):return subprocess.run(['xdotool',*map(str,args)],env=env,capture_output=True,text=True,check=True,timeout=5).stdout.strip()
            def find():
                assert process.poll() is None
                out=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],env=env,capture_output=True,text=True)
                return out.stdout.splitlines()[-1] if out.returncode==0 else None
            window=wait_until(find);command('windowfocus',window)
            wait_until(lambda:(root/'gui.log').read_text().count('receiving snapshot')==count)
            def key(value):command('keydown','--window',window,value);time.sleep(.06);command('keyup','--window',window,value);time.sleep(.1)
            def capture(name):
                stamp=time.time_ns();key('F12');image=wait_until(lambda:next((f for f in root.glob('screenshot*.png') if f.stat().st_mtime_ns>=stamp),None))
                target=ROOT/'.local/artifacts/attachment'/str(count)/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(image,target)
            capture('receiving.png')
            command('type','--window',window,'--delay',20,'x');time.sleep(.1)
            assert not (root/'f0.input').exists(),'input reached an unready terminal'
            key('super+alt+slash');capture('responsive-help.png');key('Escape')
            if count>1:key('super+alt+Right');key('super+alt+Left')
            release.set();wait_until(lambda:len(ready)==count,timeout=15)
            wait_until(lambda:(root/'gui.log').read_text().count('restored terminal snapshot')==count,timeout=15)
            time.sleep(.4);capture('restored.png')
            # Screenshot F12 is a synthetic fixture key; clear any escape echo.
            (root/'f0.input').write_bytes(b'')
            command('type','--window',window,'--delay',20,'z')
            wait_until(lambda:b'z' in (root/'f0.input').read_bytes())
            assert b'x' not in (root/'f0.input').read_bytes() and not errors,errors
            close_window(window,env);assert process.wait(timeout=5)==0
            print(f'PASS: {count} real terminals, slow snapshot feedback, help/navigation while receiving, READY input guard, staged restore and live input ({time.monotonic()-started:.2f}s including fixtures)')
        finally:
            stop.set();release.set()
            for connection in connections:
                try:connection.shutdown(socket.SHUT_RDWR)
                except OSError:pass
                connection.close()
            for listener in listeners:listener.close()
            for source in locals().get('sources',[]):subprocess.run([str(runtime),'stop',str(source)],capture_output=True,timeout=15)
            for process in reversed(processes):
                if process.poll() is None:process.terminate()
                process.wait(timeout=5)
            if sys.exc_info()[0]:log.seek(0);print(log.read()[-5000:],file=sys.stderr)
