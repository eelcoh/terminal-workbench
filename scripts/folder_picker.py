#!/usr/bin/env python3
"""Client-only native folder choice. PICK/CANCEL/ERROR replies never save state."""
from pathlib import Path
import argparse
import os
import sys
from workspace_catalog import directory_value

def initial_directory(value, cwd):
    path=Path(value).expanduser() if value else Path(cwd)
    if not path.is_absolute():path=Path(cwd)/path
    path=path.resolve()
    while not path.is_dir() and path!=path.parent:path=path.parent
    if not path.is_dir() or not os.access(path,os.R_OK|os.X_OK):path=Path.home()
    return path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--initial',default='')
    parser.add_argument('--cwd',type=Path,default=Path.cwd())
    args=parser.parse_args()
    try:
        if sys.platform=='darwin':from platform_dialogs.macos import choose
        elif sys.platform.startswith('linux'):from platform_dialogs.linux import choose
        else:raise ValueError('Folder browsing is unavailable on this platform')
        selected=choose(initial_directory(args.initial,args.cwd))
        if selected is None:print('CANCEL',flush=True)
        else:print('PICK\t'+directory_value(selected,args.cwd,available=True),flush=True)
        return 0
    except (ValueError,OSError,UnicodeError) as error:
        print('ERROR\t'+str(error).replace('\n',' ').replace('\r',' ').replace('\t',' ')[:240],flush=True)
        return 1

if __name__=='__main__':sys.exit(main())
