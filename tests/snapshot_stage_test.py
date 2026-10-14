#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-stage-') as temporary:
    executable=Path(temporary)/'stage'
    command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'spikes/ghostling'),
        '-I',str(root/'.local/runtime-vt/include'),str(root/'tests/snapshot_stage_test.c'),
        str(root/'.local/runtime-vt/lib/libghostty-vt.a'),'-o',str(executable),'-lm','-lpthread']
    if sys.platform.startswith('linux'):command.append('-ldl')
    subprocess.run(command,check=True);subprocess.run([str(executable)],check=True)
