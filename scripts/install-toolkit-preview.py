#!/usr/bin/env python3
"""Install a local preview alongside previous builds; activation stays explicit."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import tempfile
import uuid

MAC_APP='Terminal Workbench Preview.app'


def installation_root(environment,platform):
    home=Path(environment.get('HOME',str(Path.home()))).expanduser()
    if platform=='darwin':return home/'Applications/Terminal Workbench Previews'
    base=Path(environment.get('XDG_DATA_HOME',str(home/'.local/share'))).expanduser()
    if not base.is_absolute():raise ValueError('XDG_DATA_HOME must be an absolute path')
    return base/'terminal-workbench/previews'


def source_payload(source,platform):
    source=source.expanduser().resolve(strict=True)
    if platform=='darwin':
        app=source if source.name==MAC_APP else source/MAC_APP
        payload=app/'Contents/Resources/workbench'
        if not app.is_dir():raise ValueError('Select the staged preview directory or Terminal Workbench Preview.app')
        source=app
    else:payload=source
    metadata=json.loads((payload/'preview.json').read_text())
    if not isinstance(metadata,dict) or metadata.get('format')!=1:raise ValueError('Unsupported preview metadata')
    if metadata.get('platform')!=platform:raise ValueError('Preview platform does not match this machine')
    revision=metadata.get('source')
    if not isinstance(revision,str) or len(revision)!=40 or any(c not in '0123456789abcdef' for c in revision):
        raise ValueError('Preview source revision is missing or invalid')
    for relative in ('terminal-workbench','libexec/band-session','scripts/toolkit-ui.py'):
        if not (payload/relative).is_file():raise ValueError('Preview is incomplete: '+relative)
    viewer=source/'Contents/MacOS/band-ui' if platform=='darwin' else payload/'libexec/band-ui'
    executables=[payload/'terminal-workbench',payload/'libexec/band-session',viewer]
    if platform=='darwin':
        executables.append(source/'Contents/MacOS/workbench-preview')
        if not (source/'Contents/Info.plist').is_file() or not (payload/'python.path').is_file():
            raise ValueError('Preview Mac launch metadata is incomplete')
    for path in executables:
        if not path.is_file() or not os.access(path,os.X_OK):raise ValueError('Preview executable is missing or not executable: '+str(path))
    return source,metadata


def install(source,root,*,platform=None):
    platform=sys.platform if platform is None else platform
    if platform!='darwin' and not platform.startswith('linux'):raise ValueError('Preview installation supports Mac and Linux')
    source,metadata=source_payload(source,platform)
    root=root.expanduser().resolve()
    if root==source or root.is_relative_to(source):raise ValueError('Installation directory must be outside the source preview')
    root.mkdir(parents=True,exist_ok=True)
    # No old installation is selected, renamed, modified or removed. Even the
    # same commit can have several independent installs (including dirty builds).
    identity='preview-'+metadata['source'][:12]+'-'+uuid.uuid4().hex[:12]
    destination=root/identity
    temporary=Path(tempfile.mkdtemp(prefix='.install-',dir=root))
    try:
        copied=temporary/MAC_APP if platform=='darwin' else temporary/'payload'
        shutil.copytree(source,copied,symlinks=True)
        if platform=='darwin':
            payload=copied/'Contents/Resources/workbench'
            relative_launcher=Path(MAC_APP)/'Contents/Resources/workbench/terminal-workbench'
        else:
            payload=copied
            relative_launcher=Path('payload/terminal-workbench')
        # Installation evidence lives outside the signed app/copied payload.
        (temporary/'installation.json').write_text(json.dumps(dict(schema=1,source=str(source),
            revision=metadata['source'],source_dirty=metadata.get('source_dirty',False),
            payload=str(payload.relative_to(temporary))),indent=2)+'\n')
        if destination.exists():raise ValueError('New installation directory already exists; retry')
        os.rename(temporary,destination)
        return destination,destination/relative_launcher
    finally:
        if temporary.exists():shutil.rmtree(temporary)


def describe(directory,launcher,*,verified=False):
    command=shlex.quote(str(launcher))
    heading='Check again if needed' if verified else 'Check this installed copy'
    print(f'Preview installed alongside existing builds:\n  {directory}\nInstallation ID: {directory.name}\n\n{heading}:\n  {command} --check\n\nClose preview windows, then activate:\n  {command} --activate\n\nOpen a window:')
    if sys.platform=='darwin':print('  open -n '+shlex.quote(str(directory/MAC_APP)))
    else:print('  '+command)
    if sys.platform.startswith('linux'):
        print(f'\nOptional app-menu registration:\n  {command} --install-desktop-entry')
    print('\nExisting builds and sessions were left in place. Nothing was activated or opened.')



def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path,help='Staged preview directory or Mac preview app')
    parser.add_argument('--root',type=Path,help='Parent directory for separate preview installations')
    parser.add_argument('--verify',action='store_true',help='Run the copied launcher check and save results; do not activate or open')
    args=parser.parse_args()
    try:
        directory,launcher=install(args.source,args.root or installation_root(os.environ,sys.platform))
    except (OSError,ValueError) as error:
        parser.exit(1,f'Preview installation failed: {error}\n')
    if args.verify:
        from preview_install_check import verify
        try:receipt=verify(directory,launcher)
        except (OSError,ValueError) as error:parser.exit(1,f'Installed verification failed: {error}\nNothing was activated or opened.\n')
        print(f'Installed preview verified. Results: {receipt}')
    describe(directory,launcher,verified=args.verify)


if __name__=='__main__':main()
