#!/usr/bin/env python3
"""Window slot leases survive a detached launcher and release on viewer exit."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'scripts'))
from window_slots import acquire_window_slot

with tempfile.TemporaryDirectory(prefix='wb-slots-') as temporary:
    state=Path(temporary)
    one, path=acquire_window_slot(state)
    assert path.name=='1.view'
    child=subprocess.Popen([sys.executable,'-c','import sys;sys.stdin.read()'],pass_fds=(one,),stdin=subprocess.PIPE)
    os.close(one)
    try:
        two,path=acquire_window_slot(state); assert path.name=='2.view'; os.close(two)
        # Concurrent allocators never share a live slot.
        processes=[subprocess.Popen([sys.executable,'-c',
            'import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from window_slots import acquire_window_slot;fd,path=acquire_window_slot(Path(sys.argv[2]));print(path.name,flush=True);sys.stdin.read()',
            str(Path(__file__).resolve().parent.parent/'scripts'),temporary],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) for _ in range(6)]
        try:
            slots=[process.stdout.readline().strip() for process in processes]
            assert len(set(slots))==6 and '1.view' not in slots,slots
        finally:
            for process in processes: process.communicate('',timeout=5); assert process.returncode==0
        child.kill(); child.wait(timeout=5)
        one,path=acquire_window_slot(state); assert path.name=='1.view'; os.close(one)
        (state/'windows/1.lock').unlink(); (state/'windows/1.lock').symlink_to(state/'other')
        try:acquire_window_slot(state)
        except OSError:pass
        else:raise AssertionError('symlink lease accepted')
    finally:
        if child.poll() is None:child.kill();child.wait(timeout=5)
print('PASS: detached lease ownership, concurrent isolation, crash release, stable slot reuse, unsafe lock refusal')
