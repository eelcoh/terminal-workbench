"""Controlled disposable PTY writer for the Qt live-runtime benchmark."""
import json
import os
from pathlib import Path
import sys
import time
import tty

root=Path(sys.argv[1]);identity=int(sys.argv[2]);tty.setraw(0)
def atomic(path,data):
    temporary=path.with_suffix('.tmp');temporary.write_text(json.dumps(data));temporary.replace(path)
def write(data):
    offset=0
    while offset<len(data):offset+=os.write(1,data[offset:])
write(b'\x1b[?25l\x1b[2J\x1b[H'+f'LIVE READY {identity:02d}'.encode())
atomic(root/'ready.json',{'pid':os.getpid()})
seen=-1
while True:
    try:job=json.loads((root.parent/'job.json').read_text())
    except (FileNotFoundError,ValueError):time.sleep(.01);continue
    generation=job['generation']
    if generation==seen:time.sleep(.01);continue
    seen=generation;start=time.monotonic();total=0
    for frame in range(job['frames']):
        lines=(12,) if job['scenario']=='one-row' else range(1,25)
        data=b''
        for line in lines:
            prefix=f'P{identity:02d} G{generation:02d} F{frame:03d} R{line:02d} '
            text=(prefix+chr(65+frame%26)*80)[:80]
            data+=f'\x1b[{line};1H'.encode()+text.encode()
        write(data);total+=len(data)
        time.sleep(max(0,start+(frame+1)*job['seconds']/job['frames']-time.monotonic()))
    marker=f'WB-DONE-G{generation}-P{identity:02d}'
    data=b'\x1b[24;1H'+marker.ljust(80).encode();write(data);total+=len(data)
    atomic(root/f'done-{generation}.json',{'bytes':total,'marker':marker,'wall_seconds':time.monotonic()-start})
