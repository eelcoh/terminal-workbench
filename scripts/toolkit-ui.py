"""Development launcher: Qt with private disposable sessions, or the checkout's saved sessions (--saved).

Releases open windows through `band-session open` instead; see docs/packaging.md.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from catalog_files import load
from session_service import prepare_directory,request
from window_slots import acquire_window_slot

ROOT=Path(__file__).resolve().parent.parent

def config_directory(runtime):
    """Preferences directory from the runtime; preparing copies earlier Mac settings once."""
    command=[str(runtime),'config-path','--prepare']
    return Path(subprocess.run(command,check=True,capture_output=True,text=True).stdout.rstrip('\n')).parent

def configuration(state,runtime,cwd,data,preferences,saved=False):
    platform = 'macos' if sys.platform == 'darwin' else 'linux'
    return dict(state=str(state),runtime=str(runtime),cwd=str(cwd),catalog=data,
                build_info=dict(kind='development checkout',payload_root=str(ROOT)),
                preferences=str(preferences/'config.conf'),
                font_overrides=str(preferences/'ui'/platform/'terminal-font.json'),
                shortcut_overrides=str(preferences/'ui'/platform/'shortcuts.conf'),
                theme_directory=str(preferences/'themes'),theme_store=str(preferences/'themes/imports'),
                saved_session=saved,catalog_updates=saved)


def saved_window(runtime,viewer,preferences):
    state=Path(os.environ.get('BAND_STATE_DIR',str(ROOT/'.local/sessions'))).expanduser().resolve()
    cwd=ROOT
    # Match the working viewer's service configuration; initialization never
    # starts stopped saved terminals or takes ownership from another window.
    prepare_directory(state)
    views=state/'qt';views.mkdir(mode=0o700,exist_ok=True)
    lease,view=acquire_window_slot(views)
    try:
        data=request(state,runtime,cwd)['data']
        with tempfile.TemporaryDirectory(prefix='wb-qt-window-',dir='/tmp') as directory:
            config=Path(directory)/'ui.json';settings=configuration(state,runtime,cwd,data,preferences,True)
            settings['view_state']=str(view.with_suffix('.json'));settings['viewer']=str(viewer)
            config.write_text(json.dumps(settings))
            print(f'Qt saved sessions: {state}. Closing detaches; shells and workspaces remain.',flush=True)
            print(f'Qt window view: {settings["view_state"]}',flush=True)
            return subprocess.run([str(viewer),str(config)],pass_fds=(lease,)).returncode
    finally:
        os.close(lease)



parser=argparse.ArgumentParser(description='Launch Qt from the checkout with disposable or saved development sessions.')
parser.add_argument('runtime',type=Path);parser.add_argument('viewer',type=Path);parser.add_argument('--saved',action='store_true')
args=parser.parse_args();runtime=args.runtime.resolve();viewer=args.viewer.resolve()
preferences=config_directory(runtime)
if args.saved:
    sys.exit(saved_window(runtime,viewer,preferences))
with tempfile.TemporaryDirectory(prefix='wb-ui-',dir='/tmp') as directory:
    state=Path(directory);cwd=Path.home().resolve()
    def call(method='initialize',identity='',title=''):
        return request(state,runtime,cwd,method,identity,title)['data']
    try:
        data=call()
        for workspace,title in zip(list(data['workspaces']),('Workbench','Side project')):
            call('workspace.rename',workspace['id'],title)
            data=call('terminal.create',workspace['id'],'Shell')
        data=call('workspace.create',title='Empty workspace');empty=data['workspaces'][-1]
        data=call('terminal.close',empty['terminals'][0]['id'])
        config=state/'ui.json';settings=configuration(state,runtime,cwd,data,preferences);settings['viewer']=str(viewer)
        config.write_text(json.dumps(settings))
        print('UI study: private disposable sessions. Closing stops every shell created in this study.',flush=True)
        subprocess.run([str(viewer),str(config)],check=True)
    finally:
        # Include shells created after launch and sockets from a partial creation.
        try:
            if (state/'workspaces.json').exists():
                ids=[t['id'] for w in load(state)['workspaces'] for t in w['terminals']]
                if ids:request(state,runtime,cwd,'session.stop',terminal_ids=ids)
        finally:
            for sock in state.glob('*.sock'):
                subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=12)
            request(state,runtime,cwd,'service.shutdown')
