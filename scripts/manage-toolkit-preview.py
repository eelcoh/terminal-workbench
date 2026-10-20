#!/usr/bin/env python3
"""Check, activate or open one managed installation by exact ID or unique prefix."""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys

from preview_install_check import verify

spec=importlib.util.spec_from_file_location('preview_inventory',Path(__file__).with_name('list-toolkit-previews.py'))
inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)


def select(root,identity,*,platform=None):
    platform=sys.platform if platform is None else platform
    if not re.fullmatch(r'[a-zA-Z0-9_-]{1,100}',identity):raise ValueError('Use an installation ID or unique prefix from toolkit-preview-list')
    prefix=identity if identity.startswith('preview-') else 'preview-'+identity
    root=root.expanduser().resolve()
    choices=[]
    if root.is_dir():
        choices=[path for path in root.iterdir() if path.name.startswith(prefix) and path.name.startswith('preview-') and not path.is_symlink() and path.is_dir()]
    exact=[path for path in choices if path.name==prefix]
    if exact:choices=exact
    if not choices:raise ValueError('No matching managed preview; run just toolkit-preview-list')
    if len(choices)!=1:raise ValueError('Ambiguous preview ID; choose one of: '+', '.join(sorted(path.name for path in choices)))
    record=inventory.record(choices[0])
    if record['platform']!=platform:raise ValueError('Installed preview platform does not match this machine')
    return record


def execute(action,record,*,platform=None):
    platform=sys.platform if platform is None else platform
    payload=Path(record['payload']);launcher=payload/'terminal-workbench'
    if not launcher.is_file() or not os.access(launcher,os.X_OK):raise ValueError('Installed preview launcher is missing or not executable')
    if action=='check':
        receipt=verify(Path(record['directory']),launcher)
        print(f'Installed preview verified. Results: {receipt}')
        return 0
    if action=='activate':arguments=[str(launcher),'--activate']
    elif action=='open':
        if platform=='darwin':from platform_launchers.macos import preview_open_command
        elif platform.startswith('linux'):from platform_launchers.linux import preview_open_command
        else:raise ValueError('Preview opening supports Mac and Linux')
        arguments=preview_open_command(payload)
    else:raise ValueError('Unknown preview action')
    return subprocess.run(arguments,check=False).returncode


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('check','activate','open'))
    parser.add_argument('identity',help='Installed directory name or unique prefix from toolkit-preview-list')
    parser.add_argument('--root',type=Path,help='Custom installation parent (same as installer --root)')
    args=parser.parse_args()
    try:
        record=select(args.root or inventory.installer.installation_root(os.environ,sys.platform),args.identity)
        code=execute(args.action,record)
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:parser.exit(1,f'Preview action failed: {error}\n')
    sys.exit(code)


if __name__=='__main__':main()
