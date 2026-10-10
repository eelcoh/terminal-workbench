#!/usr/bin/env python3
"""Atomic snapshot/live boundary, negotiation and attachment after replay overflow."""
from pathlib import Path
import os, socket, subprocess, sys, tempfile, time
from session_smoke import attach, send, expect, read_frame, wait_until
REVISION=b'f64f4aca2c29b554d111b36c3d946a9bddd159ff'

def snapshot_attach(path, revision=REVISION):
    stream=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); stream.settimeout(5)
    stream.connect(str(path.with_suffix('.ctl'))); stream.sendall(b'ATS1\n'+revision)
    return stream

def collect(stream):
    kind,payload=read_frame(stream); assert kind==7 and len(payload)==4,(kind,payload)
    size=int.from_bytes(payload,'big'); assert 0<size<=64*1024*1024
    snapshot=bytearray()
    while True:
        kind,payload=read_frame(stream)
        if kind==9: assert not payload; break
        assert kind==8 and payload and len(snapshot)+len(payload)<=size
        snapshot.extend(payload)
    assert len(snapshot)==size and snapshot.startswith(b'GHOSTSNP')
    assert read_frame(stream)==(3,b'')
    return snapshot

def main():
    binary=str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='wb-snapshot-') as directory:
        root=Path(directory); path=root/'session.sock'
        with (root/'runtime.log').open('w+') as log:
            process=subprocess.Popen([binary,str(path),'/bin/sh'],cwd=root,stdout=log,stderr=log)
            stream=None
            try:
                wait_until(path.exists)
                wrong=snapshot_attach(path,b'0'*40); assert read_frame(wrong)[0]==4; wrong.close()
                stream,_,_=attach(path)
                send(stream,16,b"keep=saved; printf '%s' \"$$\" > pid; head -c 17825792 /dev/zero; printf '\\033[2J\\033[HPRIMARY_SNAPSHOT\\033[?1049h\\033[2J\\033[HALTERNATE_SNAPSHOT'; touch done\n")
                wait_until(lambda:(root/'done').exists(),timeout=15)
                wait_until(lambda:b'ALTERNATE_SNAPSHOT' in subprocess.check_output([binary,'read',str(path)],timeout=5))
                pid=int((root/'pid').read_text()); stream.close(); stream=None; time.sleep(.15)
                old=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);old.settimeout(5);old.connect(str(path));assert read_frame(old)[0]==4;old.close()
                stream=snapshot_attach(path); snapshot=collect(stream)
                assert b'PRIMARY_SNAPSHOT' in snapshot and b'ALTERNATE_SNAPSHOT' in snapshot
                busy=snapshot_attach(path);assert read_frame(busy)[0]==4;busy.close()
                time.sleep(1.2)  # The control-handshake deadline must not expire an idle viewer.
                send(stream,16,b"printf '\\033[?1049lLIVE_BOUNDARY:%s:%s\\n' \"$$\" \"$keep\"\n")
                match=expect(stream,rb'LIVE_BOUNDARY:(\d+):saved\r?\n'); assert int(match[1])==pid
                stream.close();stream=None;time.sleep(.15)
                # A second capture must contain the live continuation exactly once.
                stream=snapshot_attach(path); second=collect(stream)
                assert second.count(f'LIVE_BOUNDARY:{pid}:saved'.encode()) == 1
                os.kill(pid,0)
                stream.close();stream=None
                subprocess.run([binary,'stop',str(path)],check=True,timeout=12);assert process.wait(timeout=5)==0
            finally:
                if stream:stream.close()
                if process.poll() is None:
                    subprocess.run([binary,'stop',str(path)],capture_output=True,timeout=12)
                    if process.poll() is None:process.terminate()
                    process.wait(timeout=5)
                if sys.exc_info()[0]:log.flush();print((root/'runtime.log').read_text(),file=sys.stderr)
    print('PASS: pinned negotiation, busy/legacy isolation, snapshot attachment beyond 16 MiB, both screens, live boundary and same PID/state reconnect')
if __name__=='__main__':main()
