#!/usr/bin/env python3
"""Stage a same-machine Qt preview. System Qt and Python are still required."""
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parent.parent
MODULES=('toolkit-ui.py','installed_paths.py','config_paths.py','window_slots.py',
         'session_service.py','workspace_catalog.py','session_shutdown.py',
         'terminal_lifecycle.py','workbench-agent.py','agent-hook.py')


def stage(parent,runtime,viewer):
    parent.mkdir(parents=True,exist_ok=True)
    # Every stage is new: never replace files used by an existing session service.
    directory=Path(tempfile.mkdtemp(prefix='qt-preview-',dir=parent))
    try:
        scripts=directory/'scripts';scripts.mkdir()
        for name in MODULES:shutil.copy2(ROOT/'scripts'/name,scripts/name)
        executable=directory/'libexec';executable.mkdir()
        shutil.copy2(runtime,executable/'workbench-session')
        if sys.platform=='darwin':
            app=viewer.parents[2]
            if app.suffix!='.app' or viewer.name!='workbench-ui-study':
                raise ValueError('Expected the workbench-ui-study macOS application bundle')
            shutil.copytree(app,executable/'workbench-ui-study.app',symlinks=True)
            relative_viewer='libexec/workbench-ui-study.app/Contents/MacOS/workbench-ui-study'
        else:
            shutil.copy2(viewer,executable/'workbench-ui-study')
            relative_viewer='libexec/workbench-ui-study'
        # Retain the helper layout expected by the session service. All launch
        # scripts resolve their own location; no generated checkout paths escape.
        agents=directory/'.local/bin';agents.mkdir(parents=True)
        for agent in ('codex','claude','copilot'):
            wrapper=agents/('wb-'+agent)
            wrapper.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)\nexec python3 -B "$root/scripts/workbench-agent.py" '+agent+' "$@"\n')
            wrapper.chmod(0o755)
        hook=agents/'wb-agent-hook'
        hook.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)\nexec python3 -B "$root/scripts/agent-hook.py" "$@"\n')
        hook.chmod(0o755)
        plugin=directory/'.local/integrations/copilot';plugin.mkdir(parents=True)
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
        launcher=directory/'terminal-workbench'
        launcher.write_text('#!/bin/sh\nset -eu\nexport PYTHONDONTWRITEBYTECODE=1\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec python3 -B "$root/scripts/toolkit-ui.py" "$root/libexec/workbench-session" "$root/'+relative_viewer+'" --saved --installed "$@"\n')
        launcher.chmod(0o755)
        notices=directory/'notices';notices.mkdir()
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
        (directory/'preview.json').write_text(json.dumps(dict(format=1,source=revision,source_dirty=dirty,platform=sys.platform,architecture=platform.machine(),dependencies='System Python 3 and the Qt installation used to build the viewer; not bundled'),indent=2)+'\n')
        shutil.copy2(ROOT/'docs/qt-staged-preview.md',directory/'README.md')
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
    args=parser.parse_args()
    directory=stage(args.parent.expanduser().resolve(),args.runtime.resolve(strict=True),args.viewer.resolve(strict=True))
    print(f'Staged Qt preview: {directory}\nLaunch: "{directory}/terminal-workbench"\nSystem Python and Qt are required. Move this directory before its first launch.')


if __name__=='__main__':main()
