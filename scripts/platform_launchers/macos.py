"""Create a Finder bundle around the staged Qt viewer and its Python helpers."""
from pathlib import Path
import os
import plistlib
import shutil
import subprocess
import tempfile
import time
from .macos_tools import progress,run_tool

NAME='Terminal Workbench Preview.app'


def prepare(directory,viewer):
    app=viewer.parents[2]
    if app.suffix!='.app' or viewer.name!='band-ui':
        raise ValueError('Expected the band-ui macOS application bundle')
    bundle=directory/NAME
    progress(f'Copying Mac application into {bundle}')
    shutil.copytree(app,bundle,symlinks=True)
    payload=bundle/'Contents/Resources/workbench'
    payload.mkdir(parents=True)
    return payload,'../../MacOS/band-ui'


def finalize(directory,payload,bootstrap,python,*,sign=True,qt_deployment=None,stage_started=None):
    started=stage_started if stage_started is not None else time.monotonic()
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
    phases={'copy_and_setup':time.monotonic()-started}
    report=qt_deployment(bundle,payload) if qt_deployment is not None else {}
    if isinstance(report,dict):phases.update(report.get('phases_seconds',{}))
    if sign:
        sign_started=time.monotonic()
        descriptor,log=tempfile.mkstemp(prefix='qt-sign-',suffix='.log',dir=directory.parent)
        try:
            with os.fdopen(descriptor,'w') as output:
                run_tool(['/usr/bin/codesign','--force','--deep','--sign','-',str(bundle)],
                    output,'Signing Mac preview',log,timeout=120)
        except (OSError,ValueError,subprocess.SubprocessError) as error:
            raise ValueError(f'Mac signing failed: {error}. Signing log: {log}') from error
        phases['signing']=time.monotonic()-sign_started
    launcher=directory/'terminal-workbench'
    launcher.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$root/'+NAME+'/Contents/Resources/workbench/terminal-workbench" "$@"\n')
    launcher.chmod(0o755)
    if qt_deployment is not None:
        from .bundle_metrics import publish
        publish(directory,bundle,started,phases,report or {},signed=sign)


def show_error(message):
    subprocess.run(['/usr/bin/osascript','-e',
        'on run argv\n display alert "Terminal Workbench could not start" message (item 1 of argv) as critical\nend run',
        message],check=False)


def verify_signature(bundle):
    """Verify the existing staged signature without modifying the app."""
    try:
        reply=subprocess.run(['/usr/bin/codesign','--verify','--deep','--strict','--verbose=2',str(bundle)],
                             capture_output=True,text=True,timeout=30)
        return dict(ok=reply.returncode==0,exit_code=reply.returncode,stderr=reply.stderr)
    except (OSError,subprocess.SubprocessError) as error:
        return dict(ok=False,error=str(error))


def system_dependency_path(path):
    """Conventional platform library locations, after resolving path aliases."""
    return any(path.is_relative_to(root) for root in ('/System/Library','/usr/lib'))


def preview_open_command(payload):
    """Open the explicitly selected installed preview without service activation."""
    return ['/usr/bin/open','-n',str(payload.parents[2])]
