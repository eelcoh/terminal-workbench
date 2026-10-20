#!/usr/bin/env python3
"""Stage a same-machine Qt preview, optionally deploying Qt on Mac. Python remains external."""
import argparse
import importlib.util
import json
import os
import shlex
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time

ROOT=Path(__file__).resolve().parent.parent
MODULES=('toolkit-ui.py','installed_paths.py','config_paths.py','window_slots.py',
         'session_service.py','workspace_catalog.py','session_shutdown.py',
         'terminal_lifecycle.py','preview_check.py','preview_service_check.py','workbench-agent.py','agent-hook.py','desktop_launch.py','preview_activation.py')


def stage(parent,runtime,viewer,bootstrap=None,*,sign=True,bundle_qt=False,qt_tools=None,qt_notices=None,selective_qml=False,qml_deploy_script=None):
    stage_started=time.monotonic()
    deployment=None
    if selective_qml and not bundle_qt:raise ValueError('Selective QML deployment requires a Mac Qt bundle')
    if selective_qml and (qml_deploy_script is None or not qml_deploy_script.is_file()):raise ValueError('QML deployment script is missing; rebuild with just toolkit-bundle-selective')
    if bundle_qt:
        if sys.platform!='darwin':raise ValueError('Qt bundling currently requires a native Mac build; Linux uses toolkit-stage')
        from platform_launchers.macos_deploy import toolchain,deploy
        if qt_tools is None:raise ValueError('Qt deployment tool information is missing; run just toolkit-bundle')
        print('Checking Qt deployment tools…',flush=True)
        info=toolchain(qt_tools)
        deployment=lambda bundle,payload:deploy(bundle,payload,info,ROOT/'native/macos/qt-deployment',notice_directory=qt_notices,qml_deploy_script=qml_deploy_script if selective_qml else None)
    parent.mkdir(parents=True,exist_ok=True)
    # Every stage is new: never replace files used by an existing session service.
    directory=Path(tempfile.mkdtemp(prefix='qt-preview-',dir=parent))
    try:
        print(f'Staging preview: {directory}',flush=True)
        payload=directory
        relative_viewer='libexec/workbench-ui-study'
        if sys.platform=='darwin':
            from platform_launchers.macos import prepare
            payload,relative_viewer=prepare(directory,viewer)
        print('Copying runtime, launch helpers and notices…',flush=True)
        scripts=payload/'scripts';scripts.mkdir()
        for name in MODULES:shutil.copy2(ROOT/'scripts'/name,scripts/name)
        shutil.copytree(ROOT/'scripts/platform_launchers',scripts/'platform_launchers',ignore=shutil.ignore_patterns('__pycache__'))
        executable=payload/'libexec';executable.mkdir()
        shutil.copy2(runtime,executable/'workbench-session')
        if sys.platform!='darwin':shutil.copy2(viewer,executable/'workbench-ui-study')
        # Retain the helper layout expected by the session service. All launch
        # scripts resolve their own location; no generated checkout paths escape.
        agents=payload/'.local/bin';agents.mkdir(parents=True)
        for agent in ('codex','claude','copilot'):
            wrapper=agents/('wb-'+agent)
            wrapper.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)\nexec '+shlex.quote(sys.executable)+' -B "$root/scripts/workbench-agent.py" '+agent+' "$@"\n')
            wrapper.chmod(0o755)
        hook=agents/'wb-agent-hook'
        hook.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)\nexec '+shlex.quote(sys.executable)+' -B "$root/scripts/agent-hook.py" "$@"\n')
        hook.chmod(0o755)
        plugin=payload/'.local/integrations/copilot';plugin.mkdir(parents=True)
        # These hooks run inside a Workbench shell with WORKBENCH_AGENT_BIN set
        # by the runtime, so the plugin stays valid after relocating the preview.
        manifest={'name':'workbench-observers','version':'0.1.0','license':'MIT',
                  'description':'Optional local Workbench lifecycle status observers','hooks':'hooks.json'}
        events=('sessionStart','sessionEnd','userPromptSubmitted','preToolUse','postToolUse',
                'postToolUseFailure','agentStop','errorOccurred','notification')
        hooks={'version':1,'hooks':{event:[{'type':'command',
            'bash':'"$WORKBENCH_AGENT_BIN/wb-agent-hook" copilot '+event,'timeoutSec':2}] for event in events}}
        for name,value in (('plugin.json',manifest),('hooks.json',hooks)):
            (plugin/name).write_text(json.dumps(value,indent=2)+'\n')
        launcher=payload/'terminal-workbench'
        launcher.write_text('#!/bin/sh\nset -eu\nexport PYTHONDONTWRITEBYTECODE=1\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec '+shlex.quote(sys.executable)+' -B "$root/scripts/toolkit-ui.py" "$root/libexec/workbench-session" "$root/'+relative_viewer+'" --saved --installed "$@"\n')
        launcher.chmod(0o755)
        notices=payload/'notices';notices.mkdir()
        for name in ('LICENSE','fonts','themes'):
            source=ROOT/name
            if source.is_dir():
                for path in source.rglob('*'):
                    if path.is_file() and path.name in ('LICENSE','OFL.txt','README.md'):
                        destination=notices/path.relative_to(ROOT);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,destination)
            else:shutil.copy2(source,notices/name)
        shutil.copy2(ROOT/'crates/runtime/native/LICENSE.libghostty-vt',notices/'LICENSE.libghostty-vt')
        revision=subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
        dirty=bool(subprocess.run(['git','status','--porcelain'],cwd=ROOT,capture_output=True,text=True,check=True).stdout)
        (payload/'preview.json').write_text(json.dumps(dict(format=1,source=revision,source_dirty=dirty,container=os.environ.get('CONTAINER_ID','') if os.environ.get('DISTROBOX_ENTER_PATH') else '',platform=sys.platform,architecture=platform.machine(),qt_bundled=bundle_qt,selective_qml=selective_qml,dependencies='Qt deployed; recorded Python and reported non-Qt dependencies remain external' if bundle_qt else 'System Python 3 and the Qt installation used to build the viewer; not bundled'),indent=2)+'\n')
        shutil.copy2(ROOT/'docs/qt-staged-preview.md',directory/'README.md')
        if payload!=directory:shutil.copy2(ROOT/'docs/qt-staged-preview.md',payload/'README.md')
        if sys.platform=='darwin':
            from platform_launchers.macos import finalize
            if bootstrap is None:raise ValueError('Mac preview bootstrap is missing; rebuild with just toolkit-stage')
            finalize(directory,payload,bootstrap,Path(sys.executable),sign=sign,qt_deployment=deployment,stage_started=stage_started)
        return directory
    except BaseException:
        shutil.rmtree(directory)
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent',type=Path,default=ROOT/'.local/staged')
    parser.add_argument('--runtime',type=Path,default=ROOT/'target/debug/workbench-session')
    default=ROOT/'.local/build/qt-input/workbench-ui-study'
    if sys.platform=='darwin':default=default.with_suffix('.app')/'Contents/MacOS/workbench-ui-study'
    parser.add_argument('--viewer',type=Path,default=default)
    parser.add_argument('--bootstrap',type=Path,default=ROOT/'.local/build/qt-input/workbench-preview-bootstrap')
    parser.add_argument('--install',action='store_true',help='Copy the completed preview into a fresh permanent installation; do not activate or open')
    parser.add_argument('--bundle-qt',action='store_true',help='Mac: deploy Qt into the app and audit static Qt linkage')
    parser.add_argument('--selective-qml',action='store_true',help='Trial: use Qt CMake import deployment before library bundling')
    parser.add_argument('--qml-deploy-script',type=Path,help='Qt-generated deployment script (normally read from the build)')
    parser.add_argument('--qt-tools',type=Path,default=ROOT/'.local/build/qt-input/workbench-qt-tools.txt')
    parser.add_argument('--qt-notices',type=Path,help='Additional installed Qt notices directory to preserve')
    args=parser.parse_args()
    if args.bundle_qt and sys.platform!='darwin':parser.error('Qt bundling currently requires a native Mac build')
    if args.qt_notices and not args.bundle_qt:parser.error('--qt-notices requires --bundle-qt')
    if args.selective_qml and not args.bundle_qt:parser.error('--selective-qml requires --bundle-qt')
    qml_script=args.qml_deploy_script
    if args.selective_qml and qml_script is None:
        qml_script=Path((ROOT/'.local/build/qt-input/workbench-qml-deploy.txt').read_text().strip())
    directory=stage(args.parent.expanduser().resolve(),args.runtime.resolve(strict=True),args.viewer.resolve(strict=True),args.bootstrap.resolve(),bundle_qt=args.bundle_qt,qt_tools=args.qt_tools.resolve(),qt_notices=args.qt_notices.expanduser().resolve() if args.qt_notices else None,selective_qml=args.selective_qml,qml_deploy_script=qml_script.resolve() if qml_script else None)
    if args.install:
        spec=importlib.util.spec_from_file_location('preview_install',ROOT/'scripts/install-toolkit-preview.py')
        installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
        try:
            installed,launcher=installer.install(directory,installer.installation_root(os.environ,sys.platform))
        except (OSError,ValueError) as error:
            parser.exit(1,f'Preview built at {directory}, but installation failed: {error}\n')
        print(f'Build complete. Source stage retained at:\n  {directory}')
        installer.describe(installed,launcher)
        return
    launch=shlex.quote(str(directory/'terminal-workbench'))
    if sys.platform=='darwin':launch='open '+shlex.quote(str(directory/'Terminal Workbench Preview.app'))
    requirements='Uses recorded Python and deployed Qt.' if args.bundle_qt else 'Uses installed Python and Qt.'
    print(f'Build complete. Preview created at:\n  {directory}\n\nOpen a window:\n  {launch}\n\nThis command builds the preview; it does not open a window. {requirements}')
    check_launcher=directory/'Terminal Workbench Preview.app/Contents/Resources/workbench/terminal-workbench' if sys.platform=='darwin' else directory/'terminal-workbench'
    print(f'Check this preview (use its new path after moving):\n  {shlex.quote(str(check_launcher))} --check')
    if args.bundle_qt:print('Qt frameworks/QML deployed. See Contents/Resources/workbench/qt-dependencies.json for remaining dependencies and native review status.')
    if sys.platform=='darwin':print('You can move Terminal Workbench Preview.app before first use, then open it in Finder.')
    else:print('Move the preview directory before its first launch. To add it to your app menu, run its launcher with --install-desktop-entry.')



if __name__=='__main__':main()
