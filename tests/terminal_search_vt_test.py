#!/usr/bin/env python3
"""Compile the search adapter against the actual pinned VT library."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-search-vt-') as directory:
    binary=str(Path(directory)/'search')
    command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-I',str(root/'spikes/qt-input/core'),'-I',str(root/'.local/runtime-vt/include'),
        str(root/'tests/terminal_search_vt_test.c'),str(root/'.local/runtime-vt/lib/libghostty-vt.a'),'-o',binary,'-lm','-lpthread']
    if sys.platform.startswith('linux'):command.append('-ldl')
    subprocess.run(command,check=True)
    subprocess.run([binary],check=True)
