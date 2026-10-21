#!/usr/bin/env python3
"""Readonly subscriptions: real runtime ownership plus bounded malformed-stream isolation."""
from pathlib import Path
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from session_smoke import attach, send, expect, wait_until
from agent_smoke import snapshot

ROOT = Path(__file__).resolve().parent.parent
WATCH = ROOT / 'scripts/agent_watch.py'

def catalog(root, ids):
    data = dict(version=1, workspaces=[dict(id='main', name='Main', terminals=[dict(id=i,name=i) for i in ids])])
    (root/'workspaces.json').write_text(json.dumps(data))

def records(path):
    return [json.loads(line) for line in path.read_text().splitlines()]

def main():
    runtime = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='wb-watch-real-') as temporary:
        root = Path(temporary);path=root/'live.sock';catalog(root,['live','missing'])
        before=(root/'workspaces.json').read_bytes()
        fixture=root/'codex';shutil.copyfile('/usr/bin/sleep',fixture);fixture.chmod(0o700)
        server=subprocess.Popen([runtime,str(path),'/bin/sh'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        viewer=watcher=None
        try:
            wait_until(path.exists);viewer,_,_=attach(path)
            send(viewer,16,f"sh -c 'echo $$ > agent.pid; exec {fixture} 60'\n".encode())
            wait_until(lambda:snapshot(runtime,path)['agent']=='codex')
            group=os.getpgid(int((root/'agent.pid').read_text()))
            env=dict(os.environ,WORKBENCH_SESSION_SOCKET=str(path),WORKBENCH_AGENT_GROUP=str(group),WORKBENCH_AGENT_RUN=uuid.uuid4().hex)
            output=root/'events'
            with output.open('w') as log:
                watcher=subprocess.Popen([sys.executable,str(WATCH),'--state-dir',str(root),'--seconds','5'],stdout=log,stderr=subprocess.PIPE,text=True)
                wait_until(lambda:any(e.get('status')=='unknown' and e['terminal_id']=='live' for e in records(output)))
                for event,status in [('UserPromptSubmit','working'),('PermissionRequest','waiting'),('Stop','ready')]:
                    hook=subprocess.run([sys.executable,str(ROOT/'scripts/agent-hook.py'),'codex'],input=json.dumps(dict(hook_event_name=event)),env=env,text=True,capture_output=True,timeout=3)
                    assert hook.returncode==0 and not hook.stdout and not hook.stderr
                    wait_until(lambda:any(e.get('status')==status for e in records(output)))
                send(viewer,16,b'\x03')
                wait_until(lambda:any(e.get('agent') is None and e.get('event')=='state' and e.get('identity_available') for e in records(output)))
                send(viewer,16,b'printf WB_WATCH_SAME_OWNER\n');expect(viewer,b'WB_WATCH_SAME_OWNER')
                _,error=watcher.communicate(timeout=8);assert watcher.returncode==0,error
            events=records(output)
            assert [e['status'] for e in events if e.get('event')=='state']==['unknown','working','waiting','ready','unknown']
            assert any(e['terminal_id']=='missing' and e['event']=='unavailable' for e in events)
            assert before==(root/'workspaces.json').read_bytes() and not (root/'catalog.lock').exists()
        finally:
            if watcher is not None and watcher.poll() is None:watcher.terminate();watcher.wait(timeout=5)
            if viewer is not None:viewer.close()
            subprocess.run([runtime,'stop',str(path)],capture_output=True,timeout=12);server.wait(timeout=5)
    with tempfile.TemporaryDirectory(prefix='wb-watch-wire-') as temporary:
        root=Path(temporary);catalog(root,['valid','broken','bad-tags','quiet','regular','linked'])
        (root/'regular.ctl').write_text('preserved')
        sockets=[];threads=[];commands=[];errors=[]
        def serve(listener,name):
            try:
                with listener.accept()[0] as probe:
                    probe.settimeout(3);assert probe.recv(64)==b'CAPS\n';probe.sendall(b'{"capabilities":[]}\n')
                with listener.accept()[0] as connection:
                    connection.settimeout(3);commands.append(connection.recv(64));assert commands[-1]==b'WAT2\n'
                    if name=='broken':connection.sendall(struct.pack('!BI',6,2**31));return
                    if name=='bad-tags':connection.sendall(struct.pack('!BI',6,2)+bytes([6,255]));return
                    if name=='quiet':time.sleep(1.3);return
                    # Fragment one frame and coalesce later frames, including a duplicate.
                    raw=b''.join(struct.pack('!BI',6,2)+bytes([6,status]) for status in (0,1,1,2,3))
                    connection.sendall(raw[:3]);time.sleep(.04);connection.sendall(raw[3:]);time.sleep(.15)
            except BaseException as error:errors.append(error)
        try:
            for name in ('valid','broken','bad-tags','quiet'):
                listener=socket.socket(socket.AF_UNIX);listener.bind(str(root/(name+'.ctl')));listener.listen();sockets.append(listener)
                thread=threading.Thread(target=serve,args=(listener,name));thread.start();threads.append(thread)
            (root/'linked.ctl').symlink_to(root/'valid.ctl')
            result=subprocess.run([sys.executable,str(WATCH),'--state-dir',str(root),'--seconds','1'],capture_output=True,text=True,timeout=5)
            assert result.returncode==0,result.stderr
            data=[json.loads(line) for line in result.stdout.splitlines()]
            assert [e['status'] for e in data if e['terminal_id']=='valid' and e['event']=='state']==['unknown','working','waiting','ready']
            assert all(e['agent']=='copilot' for e in data if e['event']=='state')
            assert {e['terminal_id'] for e in data if e['event']=='unavailable'}=={'valid','broken','bad-tags','quiet','regular','linked'}
            assert (root/'regular.ctl').read_text()=='preserved'
            for thread in threads:thread.join(timeout=4);assert not thread.is_alive()
            assert not errors,errors
            assert len(commands)==4
            bad=subprocess.run([sys.executable,str(WATCH),'--state-dir',str(root),'--seconds','nan'],capture_output=True,timeout=3)
            assert bad.returncode!=0 and not bad.stdout
        finally:
            for listener in sockets:listener.close()
    print('PASS: real lifecycle transitions, unchanged PTY owner/input/catalog, missing endpoint isolation, fragmented/coalesced/deduplicated frames, malformed bounds, EOF, unsafe sockets and duration validation')

if __name__=='__main__':main()
