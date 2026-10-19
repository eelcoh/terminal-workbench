"""Create a Finder bundle around the staged Qt viewer and its Python helpers."""
from pathlib import Path
import os
import plistlib
import shutil
import subprocess
import tempfile
from .macos_tools import progress,run_tool

NAME='Terminal Workbench Preview.app'


def prepare(directory,viewer):
    app=viewer.parents[2]
    if app.suffix!='.app' or viewer.name!='workbench-ui-study':
        raise ValueError('Expected the workbench-ui-study macOS application bundle')
    bundle=directory/NAME
    progress(f'Copying Mac application into {bundle}')
    shutil.copytree(app,bundle,symlinks=True)
    payload=bundle/'Contents/Resources/workbench'
    payload.mkdir(parents=True)
    return payload,'../../MacOS/workbench-ui-study'


def finalize(directory,payload,bootstrap,python,*,sign=True,qt_deployment=None):
    bundle=directory/NAME
    executable=bundle/'Contents/MacOS/workbench-preview'
    shutil.copy2(bootstrap,executable)
    info=bundle/'Contents/Info.plist'
    values=plistlib.loads(info.read_bytes()) if info.exists() else {}
    values.update(CFBundleExecutable='workbench-preview',CFBundleIdentifier='dev.terminal-workbench.qt-preview',
                  CFBundleName='Terminal Workbench Preview',CFBundleDisplayName='Terminal Workbench Preview',
                  CFBundlePackageType='APPL',CFBundleVersion='0.1.0',CFBundleShortVersionString='0.1.0',
                  NSHighResolutionCapable=True)
    # CMake may generate a read-only plist, and copytree retains that mode.
    # Publish a new file in the staged bundle rather than modifying the copied
    # inode (or following a copied symlink back into the build output).
    descriptor,temporary=tempfile.mkstemp(prefix='.workbench-info-',dir=info.parent)
    try:
        with os.fdopen(descriptor,'wb') as file:plistlib.dump(values,file)
        os.chmod(temporary,0o644)
        os.replace(temporary,info)
    finally:
        Path(temporary).unlink(missing_ok=True)
    if '\n' in str(python) or '\r' in str(python):raise ValueError('Python path contains a newline')
    (payload/'python.path').write_text(str(python)+'\n')
    if qt_deployment is not None:qt_deployment(bundle,payload)
    if sign:
        descriptor,log=tempfile.mkstemp(prefix='qt-sign-',suffix='.log',dir=directory.parent)
        try:
            with os.fdopen(descriptor,'w') as output:
                run_tool(['/usr/bin/codesign','--force','--deep','--sign','-',str(bundle)],
                    output,'Signing Mac preview',log,timeout=120)
        except (OSError,ValueError,subprocess.SubprocessError) as error:
            raise ValueError(f'Mac signing failed: {error}. Signing log: {log}') from error
    launcher=directory/'terminal-workbench'
    launcher.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$root/'+NAME+'/Contents/Resources/workbench/terminal-workbench" "$@"\n')
    launcher.chmod(0o755)


def show_error(message):
    subprocess.run(['/usr/bin/osascript','-e',
        'on run argv\n display alert "Terminal Workbench could not start" message (item 1 of argv) as critical\nend run',
        message],check=False)
