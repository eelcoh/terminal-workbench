"""Pi wire/ownership contracts with a named native fixture, not live CLI evidence."""
import json,os,shlex,shutil,socket,struct,subprocess,sys,tempfile,uuid
from pathlib import Path
from session_smoke import attach,send,read_frame,wait_until,expect
from agent_smoke import snapshot
runtime=str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='wb-pi-wire-') as temporary:
    root=Path(temporary);path=root/'pi.sock';fixture=root/'pi';shutil.copyfile(shutil.which('sleep'),fixture);fixture.chmod(0o700)
    with (root/'runtime.log').open('w') as log:
        server=subprocess.Popen([runtime,str(path),'/bin/sh'],cwd=root,stdout=log,stderr=log)
        viewer=None;watchers=[]
        try:
            wait_until(lambda:path.with_suffix('.ctl').exists());viewer,_,_=attach(path)
            send(viewer,16,('sh -c '+shlex.quote('echo $$ > '+shlex.quote(str(root/'agent.pid'))+'; exec '+shlex.quote(str(fixture))+' 120')+'\n').encode())
            wait_until(lambda:snapshot(runtime,path)['agent']=='pi')
            for request,kind,payload in [(b'WATCH',5,b'\xff'),(b'WAT2\n',6,b'\xff\x00'),(b'WAT3\n',6,b'\x07\x00')]:
                stream=socket.socket(socket.AF_UNIX);stream.settimeout(3);stream.connect(str(path.with_suffix('.ctl')));stream.sendall(request);watchers.append(stream)
                assert read_frame(stream)==(kind,payload)
            # Runtime metadata reports do not depend on terminal output.
            wait_until((root/'agent.pid').exists);group=os.getpgid(int((root/'agent.pid').read_text()));token=uuid.uuid4().bytes
            for sequence,state in enumerate((1,3),1):
                with socket.socket(socket.AF_UNIX) as control:
                    control.settimeout(2);control.connect(str(path.with_suffix('.ctl')));control.sendall(b'STATE'+struct.pack('!BBi16sQ',7,state,group,token,sequence));assert control.recv(16)==b'OK\n'
                assert read_frame(watchers[2])==(6,bytes([7,state]));assert read_frame(watchers[1])==(6,b'\xff\x00')
            assert snapshot(runtime,path)['status']=='ready'
            catalog={'version':1,'workspaces':[{'id':'main','name':'Main','terminals':[{'id':'pi','name':'Pi'}]}]}
            (root/'workspaces.json').write_text(json.dumps(catalog));before=(root/'workspaces.json').read_bytes()
            trace=subprocess.run([sys.executable,str(Path(__file__).resolve().parent.parent/'scripts/agent_watch.py'),'--state-dir',str(root),'--seconds','1'],capture_output=True,text=True,timeout=5)
            assert trace.returncode==0,trace.stderr
            assert any(event.get('agent')=='pi' and event.get('status')=='ready' for event in map(json.loads,trace.stdout.splitlines()))
            assert (root/'workspaces.json').read_bytes()==before

            send(viewer,16,b'\x03');wait_until(lambda:snapshot(runtime,path)['agent'] is None)
            send(viewer,16,b'printf WB_PI_OWNER_RETAINED\n');expect(viewer,b'WB_PI_OWNER_RETAINED')
        finally:
            for stream in watchers:stream.close()
            if viewer:viewer.close()
            subprocess.run([runtime,'stop',str(path)],capture_output=True,timeout=12);server.wait(timeout=5)
print('PASS: Pi identity/lifecycle on WAT3, frozen older watch fallback, retained terminal input/owner and cleared foreground identity')
