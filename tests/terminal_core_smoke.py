#!/usr/bin/env python3
"""Runtime VT state and detached query replies through real PTYs/control sockets."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, expect, send, wait_until

def main():
    binary=str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='wb-terminal-core-') as directory:
        root=Path(directory); path=root/'session.sock'; helper=root/'probe'
        helper.write_text('#!'+sys.executable+'\n'+r"""
from pathlib import Path
import os, select, time, tty
root=Path.cwd(); tty.setraw(0); (root/'child.pid').write_text(str(os.getpid()))
def receive(end):
    data=b''; deadline=time.monotonic()+5
    while not data.endswith(end):
        if not select.select([0],[],[],max(0,deadline-time.monotonic()))[0]: raise RuntimeError('terminal query stalled')
        data+=os.read(0,4096)
    return data
os.write(1,b'\x1b[2J\x1b[HPRIMARY\x1b[4;7H\x1b[6n')
(root/'cursor.reply').write_bytes(receive(b'R'))
os.write(1,b'\x1b[18t'); (root/'size.reply').write_bytes(receive(b't'))
os.write(1,b'\x1b[c'); (root/'attributes.reply').write_bytes(receive(b'c'))
os.write(1,b'\x1b[>q'); (root/'version.reply').write_bytes(receive(b'\x1b\\'))
os.write(1,b'\x1b[?1049h\x1b[2J\x1b[HALTERNATE')
(root/'ready').touch()
assert os.read(0,1)==b'X'
os.write(1,b'\x1b[6n'); (root/'attached.reply').write_bytes(receive(b'R'))
extra=os.read(0,4096) if select.select([0],[],[],.15)[0] else b''
(root/'extra.reply').write_bytes(extra); (root/'attached.done').touch()
while True: time.sleep(1)
""")
        helper.chmod(0o700)
        with (root/'runtime.log').open('w+') as log:
            process=subprocess.Popen([binary,str(path),str(helper)],cwd=root,stdout=log,stderr=log)
            connection=None
            try:
                wait_until(lambda:(root/'ready').exists(),timeout=10)
                pid=int((root/'child.pid').read_text()); inode=path.stat().st_ino
                assert (root/'cursor.reply').read_bytes()==b'\x1b[4;7R'
                assert (root/'size.reply').read_bytes()==b'\x1b[8;24;80t'
                assert (root/'attributes.reply').read_bytes().endswith(b'c')
                assert b'band' in (root/'version.reply').read_bytes()
                def read(): return subprocess.check_output([binary,'read',str(path)],timeout=5)
                wait_until(lambda:b'ALTERNATE' in read())
                export=root/'terminal.snapshot'
                subprocess.run([binary,'snapshot',str(path),str(export)],check=True,timeout=5)
                snapshot=export.read_bytes(); assert snapshot.startswith(b'GHOSTSNP') and b'PRIMARY' in snapshot and b'ALTERNATE' in snapshot
                connection,_,_=attach(path)
                assert b'ALTERNATE' in read(), 'control state read must not displace an attached viewer'
                send(connection,16,b'X')
                expect(connection,rb'\x1b\[6n')
                send(connection,16,b'\x1b[1;10R')
                wait_until(lambda:(root/'attached.done').exists())
                assert (root/'attached.reply').read_bytes()==b'\x1b[1;10R'
                assert not (root/'extra.reply').read_bytes(), 'runtime duplicated attached viewer query replies'
                os.kill(pid,0); assert path.stat().st_ino==inode
                connection.close(); connection=None
                subprocess.run([binary,'stop',str(path)],check=True,timeout=12)
                assert process.wait(timeout=5)==0
            finally:
                if connection: connection.close()
                if process.poll() is None:
                    subprocess.run([binary,'stop',str(path)],capture_output=True,timeout=12)
                    if process.poll() is None: process.terminate()
                    process.wait(timeout=5)
                if sys.exc_info()[0]: log.flush(); print((root/'runtime.log').read_text(),file=sys.stderr)
    print('PASS: detached cursor/size/device/version replies, runtime text/binary state, both screens, reads with viewer attached, reply ownership, stable PID/socket and stop')

if __name__=='__main__': main()
