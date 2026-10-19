"""Create a Finder bundle around the staged Qt viewer and its Python helpers."""
from pathlib import Path
import plistlib
import shutil
import subprocess

NAME='Terminal Workbench Preview.app'


def prepare(directory,viewer):
    app=viewer.parents[2]
    if app.suffix!='.app' or viewer.name!='workbench-ui-study':
        raise ValueError('Expected the workbench-ui-study macOS application bundle')
    bundle=directory/NAME
    shutil.copytree(app,bundle,symlinks=True)
    payload=bundle/'Contents/Resources/workbench'
    payload.mkdir(parents=True)
    return payload,'../../MacOS/workbench-ui-study'


def finalize(directory,payload,bootstrap,python,*,sign=True):
    bundle=directory/NAME
    executable=bundle/'Contents/MacOS/workbench-preview'
    shutil.copy2(bootstrap,executable)
    info=bundle/'Contents/Info.plist'
    values=plistlib.loads(info.read_bytes()) if info.exists() else {}
    values.update(CFBundleExecutable='workbench-preview',CFBundleIdentifier='dev.terminal-workbench.qt-preview',
                  CFBundleName='Terminal Workbench Preview',CFBundleDisplayName='Terminal Workbench Preview',
                  CFBundlePackageType='APPL',CFBundleVersion='0.1.0',CFBundleShortVersionString='0.1.0',
                  NSHighResolutionCapable=True)
    info.write_bytes(plistlib.dumps(values))
    if '\n' in str(python) or '\r' in str(python):raise ValueError('Python path contains a newline')
    (payload/'python.path').write_text(str(python)+'\n')
    if sign:
        subprocess.run(['/usr/bin/codesign','--force','--deep','--sign','-',str(bundle)],check=True)
    launcher=directory/'terminal-workbench'
    launcher.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$root/'+NAME+'/Contents/Resources/workbench/terminal-workbench" "$@"\n')
    launcher.chmod(0o755)


def show_error(message):
    subprocess.run(['/usr/bin/osascript','-e',
        'on run argv\n display alert "Terminal Workbench could not start" message (item 1 of argv) as critical\nend run',
        message],check=False)
