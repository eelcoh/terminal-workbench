"""Framed legacy compatibility without touching the user's saved terminals."""
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

viewer=str(Path(sys.argv[1]).resolve())
def frame(kind,data=b''):return bytes([kind])+struct.pack('>I',len(data))+data

def scenario(mode):
    with tempfile.TemporaryDirectory(prefix='wb-qt-legacy-',dir='/tmp') as temp:
        root=Path(temp);control=root/'test.ctl';replay=root/'test.sock';report=root/'report.json'
        stop=threading.Event();errors=[];controls=[];inputs=bytearray();replays=[]
        listeners=[]
        def listen(path,handler):
            listener=socket.socket(socket.AF_UNIX);listener.bind(str(path));listener.listen();listener.settimeout(.1);listeners.append(listener)
            def serve():
                try:
                    while not stop.is_set():
                        try:c,_=listener.accept()
                        except socket.timeout:continue
                        with c:
                            c.settimeout(.1);handler(c)
                except Exception as e:
                    if not stop.is_set():errors.append(e)
            t=threading.Thread(target=serve);t.start();return t
        def control_handler(c):
            hello=b''
            while len(hello)<45:hello+=c.recv(45-len(hello))
            controls.append(hello)
            if mode=='unknown':return
            data=frame(4,b'a window is already attached') if mode=='busy' else frame(7,struct.pack('>I',9))[:3]
            c.sendall(data)
        def replay_handler(c):
            replays.append(True)
            if mode=='rejected':c.sendall(frame(4,b'a window is already attached'));return
            # Terminal queries in retained history must not write to the shell.
            payload=frame(2,struct.pack('>HH',80,24))+frame(1,b'\x1b[2J\x1b[HLEGACY-RESTORED QT-KEY-PROBE\x1b[6n')
            for i in range(0,len(payload),3):c.sendall(payload[i:i+3])
            time.sleep(.15)
            try:inputs.extend(c.recv(65536))
            except socket.timeout:pass
            assert not inputs,'replay emitted input/query replies before READY'
            c.sendall(frame(3))
            while not stop.is_set():
                try:
                    data=c.recv(65536)
                    if not data:return
                    inputs.extend(data)
                except socket.timeout:continue
        threads=[listen(replay,replay_handler)]
        if mode in {'unknown','busy','partial'}:threads.append(listen(control,control_handler))
        if mode=='refused':
            stale=socket.socket(socket.AF_UNIX);stale.bind(str(control));stale.close()
        env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
        with (root/'viewer.log').open('w') as log:
            client=subprocess.Popen([viewer,'--control',str(control),'--test-report',str(report),'--test-keys'],env=env,stdout=log,stderr=log)
            try:
                deadline=time.monotonic()+8
                while True:
                    assert client.poll() is None,(mode,client.returncode,(root/'viewer.log').read_text())
                    try:data=json.loads(report.read_text())
                    except (FileNotFoundError,ValueError):data={}
                    if mode in {'busy','partial','rejected'}:
                        if data.get('status') in {'a window is already attached','Connection closed'}:break
                    elif data.get('ready') and 'LEGACY-RESTORED' in data.get('text',''):break
                    assert time.monotonic()<deadline,(mode,data,errors,(root/'viewer.log').read_text())
                    time.sleep(.03)
                if mode in {'busy','partial'}:assert not replays,(mode,'unsafe fallback after response')
                elif mode=='rejected':assert not data['ready'] and len(replays)==1
                else:
                    assert data['status']=='Attached · legacy replay' and len(replays)==1
                    def key_bytes():
                        pending=bytes(inputs);result=b''
                        while len(pending)>=5:
                            kind=pending[0];size=struct.unpack('>I',pending[1:5])[0]
                            if len(pending)<5+size:break
                            if kind==16:result+=pending[5:5+size]
                            else:assert kind==17 and size==4
                            pending=pending[5+size:]
                        return result
                    expected=b'\x1b[D\x1bb\x1bf\x1bf\x1b[C\x1b[1;5D'
                    deadline=time.monotonic()+3
                    while key_bytes()!=expected:
                        assert time.monotonic()<deadline,(mode,key_bytes(),expected)
                        time.sleep(.02)
                if controls:assert controls==[b'ATS1\nf64f4aca2c29b554d111b36c3d946a9bddd159ff']
                assert not errors,errors
            finally:
                client.terminate();client.wait(timeout=5);stop.set()
                for t in threads:t.join(timeout=2)
                for listener in listeners:listener.close()
for mode in ['missing','refused','unknown','busy','partial','rejected']:scenario(mode)
print('PASS: Qt missing/refused/older control legacy replay, fragmented history, exact input, query suppression, owner rejection and partial-response isolation')
