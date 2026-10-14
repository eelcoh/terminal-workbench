#!/usr/bin/env python3
"""Disposable foreground TUI workload for session soak checks."""
from pathlib import Path
import json,os,select,sys,termios,time,tty
root=Path(sys.argv[1]);identity=sys.argv[2]
original=termios.tcgetattr(0);tty.setraw(0)
sequence=bytes_written=unexpected=epoch=0
def publish(path,value):
    temporary=path.with_name(path.name+'.tmp');temporary.write_text(json.dumps(value));temporary.replace(path)
def receive(end):
    data=b'';deadline=time.monotonic()+5
    while not data.endswith(end):
        if not select.select([0],[],[],max(0,deadline-time.monotonic()))[0]:raise RuntimeError('detached query stalled')
        data+=os.read(0,4096)
        if len(data)>256:raise RuntimeError('unexpected query response')
    return data
try:
    while True:
        command=root/(identity+'.query')
        if command.exists():
            value=json.loads(command.read_text())
            if value['epoch']>epoch:
                epoch=value['epoch']
                os.write(1,b'\x1b[4;7H\x1b[6n');cursor=receive(b'R')
                os.write(1,b'\x1b[18t');size=receive(b't')
                extra=os.read(0,4096) if select.select([0],[],[],.05)[0] else b''
                publish(root/(identity+'.reply'),dict(epoch=epoch,cursor=cursor.hex(),size=size.hex(),extra=extra.hex()))
        if select.select([0],[],[],0)[0]:
            data=os.read(0,4096)
            if data==b'X':break
            unexpected+=len(data)
        sequence+=1
        if sequence%40==1:os.write(1,b'\x1b[?1049h' if (sequence//40)%2==0 else b'\x1b[?1049l')
        packet=('\x1b[36mSOAK:'+identity+':'+str(sequence)+'\x1b[0m '+('x'*64)+'\r\n').encode()*64
        os.write(1,packet);bytes_written+=len(packet)
        publish(root/(identity+'.progress'),dict(pid=os.getpid(),sequence=sequence,bytes=bytes_written,unexpected=unexpected))
        time.sleep(.025)
finally:
    os.write(1,b'\x1b[?1049l')
    termios.tcsetattr(0,termios.TCSANOW,original)
