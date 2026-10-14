#!/usr/bin/env python3
from pathlib import Path
import json,os,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'scripts'))
from terminal_lifecycle import detail,read_record
with tempfile.TemporaryDirectory(prefix='wb-life-record-') as directory:
    root=Path(directory);socket=root/'one.sock';path=socket.with_suffix('.lifecycle')
    record=dict(version=1,runtime_pid=123,shell_pid=456,started_ms=1,event='exited',exit_code=0,signal=None)
    def write(value):path.write_text(json.dumps(value));path.chmod(0o600)
    assert detail(socket,'stopped')['reason']=='unknown'
    write(record);assert detail(socket,'stopped')['reason']=='exited'
    assert detail(socket,'running')['reason']=='running','Old receipt overrode a live endpoint'
    assert detail(socket,'unavailable')['reason']=='unavailable'
    for bad in ({**record,'version':True},{**record,'exit_code':True},{**record,'exit_code':-1},
        {**record,'exit_code':1,'signal':'Killed'},{**record,'event':'running'},{**record,'exit_code':None,'signal':None},
        {**record,'signal':'bad\ntext'},{**record,'event':'imaginary'},{**record,'event':[]},{key:value for key,value in record.items() if key!='signal'},[],{'version':2}):
        write(bad);assert read_record(socket) is None and detail(socket,'stopped')['reason']=='unknown'
    path.write_text('x'*4097);assert read_record(socket) is None
    path.write_text('{broken');assert read_record(socket) is None
    path.write_text('['*1200+'0'+']'*1200);assert read_record(socket) is None
    write(record);path.chmod(0o666);assert read_record(socket) is None
    path.unlink();other=root/'other';other.write_text(json.dumps(record));path.symlink_to(other)
    assert read_record(socket) is None;path.unlink();os.mkfifo(path);assert read_record(socket) is None
print('PASS: missing/corrupt/oversized/unsafe evidence is unknown; live or uncertain endpoint state stays authoritative')
