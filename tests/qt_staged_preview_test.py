"""Relocated payloads use user state, retain shells and resolve their own helpers."""
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
from types import SimpleNamespace

ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tests')]
from installed_paths import session_directory
from session_service import request
from session_smoke import attach,send,wait_until
spec=importlib.util.spec_from_file_location('stage_toolkit',ROOT/'scripts/stage-toolkit.py')
staging=importlib.util.module_from_spec(spec);spec.loader.exec_module(staging)
runtime=Path(sys.argv[1]).resolve()

assert session_directory({'HOME':'/Users/fixture'})==Path('/Users/fixture/.local/state/band')
assert session_directory({'HOME':'/tmp/home','XDG_STATE_HOME':'/tmp/state'})==Path('/tmp/state/band')
assert session_directory({'BAND_STATE_DIR':'/tmp/explicit'})==Path('/tmp/explicit')
try:session_directory({'XDG_STATE_HOME':'relative'})
except ValueError:pass
else:raise AssertionError('relative XDG state was accepted')

with tempfile.TemporaryDirectory(prefix='wb-stage-',dir='/tmp') as directory:
    root=Path(directory);home=root/'home';home.mkdir();outside=root/'outside';outside.mkdir()
    fake=root/'viewer'
    fake.write_text('#!'+sys.executable+'\nimport json,os,sys\nfrom pathlib import Path\nc=json.loads(Path(sys.argv[1]).read_text());Path(os.environ["WB_STAGE_CONFIG"]).write_text(json.dumps(c))\nsys.exit(int(os.environ.get("WB_STAGE_EXIT","0")))\n')
    fake.chmod(0o755)
    # Exercise Mac bundle layout generation on either test platform. This is
    # structural coverage only; the copied native Mac viewer still needs review.
    mac_viewer=root/'Odd " name.app/Contents/MacOS/band-ui'
    mac_viewer.parent.mkdir(parents=True);shutil.copy2(fake,mac_viewer)
    native_sys=staging.sys
    try:
        staging.sys=SimpleNamespace(platform='darwin',executable=sys.executable)
        mac_stage=staging.stage(root/'mac-layout',runtime,mac_viewer,fake,sign=False)
    finally:staging.sys=native_sys
    assert (mac_stage/'Terminal Workbench Preview.app/Contents/MacOS/band-ui').is_file()
    assert (mac_stage/'Terminal Workbench Preview.app/Contents/Resources/workbench/scripts/toolkit-ui.py').is_file()
    # Use the real Mac .app copy path when running on Mac; replace only its main
    # executable with the capture viewer for lifecycle assertions.
    built=ROOT/'.local/build/qt-input/band-ui'
    if sys.platform=='darwin':built=built.with_suffix('.app')/'Contents/MacOS/band-ui'
    initial=staging.stage(root/'build location',runtime,built,ROOT/'.local/build/qt-input/workbench-preview-bootstrap')
    moved=root/'relocated preview';initial.rename(moved)
    viewer=moved/'libexec/band-ui'
    if sys.platform=='darwin':viewer=moved/'Terminal Workbench Preview.app/Contents/MacOS/band-ui'
    payload=moved/'Terminal Workbench Preview.app/Contents/Resources/workbench' if sys.platform=='darwin' else moved
    real=viewer.with_name('real-viewer');viewer.rename(real);shutil.copy2(fake,viewer)
    launcher=moved/'terminal-workbench';evidence=root/'config.json'
    env={k:v for k,v in os.environ.items() if not k.startswith(('BAND_','XDG_','WB_STAGE_'))}
    env.update(HOME=str(home),SHELL='/bin/sh',WB_STAGE_CONFIG=str(evidence),PYTHONDONTWRITEBYTECODE='1')
    state=session_directory(env)
    def launch(*args,exit_code=0,**extra):
        result=subprocess.run([str(launcher),*args],cwd=outside,env=dict(env,WB_STAGE_EXIT=str(exit_code),**extra),capture_output=True,text=True,timeout=60)
        assert result.returncode==exit_code,(result.stdout,result.stderr)
        return result
    paths=json.loads(launch('--paths').stdout)
    assert not state.exists() and not (home/'.config').exists(),'path inspection had side effects'
    assert paths['state']==str(state) and paths['cwd']==str(home)
    assert paths['configuration']==str(home/'.config/band')
    assert Path(paths['runtime']).is_relative_to(moved) and Path(paths['viewer']).is_relative_to(moved)
    assert str(ROOT) not in json.dumps(paths) and not initial.exists()
    assert all(not p.is_symlink() for p in (payload/'scripts').iterdir())
    payload_files={str(p.relative_to(moved)) for p in moved.rglob('*')}
    try:
        launch();config=json.loads(evidence.read_text())
        assert config['cwd']==str(home) and config['state']==str(state) and config['saved_session']
        assert all(w.get('directory',config['cwd'])==str(home) for w in config['catalog']['workspaces'])
        terminal=config['catalog']['workspaces'][0]['terminals'][0]['id']
        staged_runtime=Path(paths['runtime'])
        connection,_,_=attach(state/(terminal+'.sock'))
        try:
            proof=root/'shell-proof';send(connection,16,('printf "%s\\n%s" "$$" "$BAND_AGENT_BIN" > '+shlex.quote(str(proof))+'\n').encode())
            wait_until(lambda:proof.exists() and len(proof.read_text().splitlines())==2)
            pid,agent_bin=proof.read_text().splitlines();assert Path(agent_bin)==payload/'.local/bin'
        finally:connection.close()
        service=request(state,staged_runtime,home,'health')['service_pid']
        launch(exit_code=42);assert json.loads(evidence.read_text())['catalog']==config['catalog']
        os.kill(int(pid),0)
        assert request(state,staged_runtime,home,'health')['service_pid']==service
        # Every wrapper must find its copied script after relocation, preserve
        # arguments, and leave user agent policy untouched.
        commands=root/'commands';commands.mkdir()
        for agent in ('codex','claude','copilot','pi'):
            command=commands/agent
            command.write_text('#!'+sys.executable+'\nimport json,sys\nprint(json.dumps(sys.argv[1:]))\n');command.chmod(0o755)
            result=subprocess.run([str(Path(agent_bin)/('band-'+agent)),'a spaced argument'],cwd=outside,
                env=dict(env,PATH=str(commands)+os.pathsep+env['PATH'],BAND_AGENT_BIN=agent_bin,BAND_SESSION_SOCKET=str(state/(terminal+'.sock'))),capture_output=True,text=True,check=True)
            arguments=json.loads(result.stdout);assert arguments[-1]=='a spaced argument'
            assert str(ROOT) not in result.stdout and str(initial) not in result.stdout
            if agent=='pi':
                extension=Path(arguments[1]);assert arguments[0]=='--extension' and extension.is_relative_to(moved) and extension.read_bytes()==(ROOT/'integrations/pi/workbench.ts').read_bytes()
            if agent=='copilot':
                plugin=Path(arguments[1]);assert plugin.is_relative_to(moved)
                hooks=json.loads((plugin/'hooks.json').read_text())
                for entry in hooks['hooks'].values():
                    hook=subprocess.run(entry[0]['bash'],shell=True,input='{}',text=True,capture_output=True,
                        env=dict(env,BAND_AGENT_BIN=agent_bin),timeout=5)
                    assert hook.returncode==0 and hook.stdout==hook.stderr=='',(hook.stdout,hook.stderr)
        request(state,staged_runtime,home,'session.stop',terminal_ids=[terminal])
        def stopped():
            return next(t['state'] for t in request(state,staged_runtime,home,'health')['terminals'] if t['id']==terminal)=='stopped'
        wait_until(stopped);launch();assert stopped(),'installed reopen revived a stopped terminal'
        assert {str(p.relative_to(moved)) for p in moved.rglob('*')}==payload_files,'launcher wrote into the preview payload'
        if sys.platform.startswith('linux'):
            shutil.copy2(real,viewer)
            # The real installed check runs while a service/shell already exists.
            # It must neither acquire a window slot nor alter saved sessions.
            before_health=request(state,staged_runtime,home,'health')
            before_files={str(p.relative_to(state)):p.read_bytes() for p in state.rglob('*') if p.is_file() and p.name in ('workspaces.json','qt-view.json')}
            checked=json.loads(launch('--check',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software').stdout)
            assert checked['ok'],checked
            dependencies=checked['dependencies']
            assert dependencies['python']['executable']==sys.executable and not dependencies['python']['bundled'],dependencies
            assert dependencies['build_python']['available'] and dependencies['build_python']['executable']==sys.executable,dependencies
            assert dependencies['viewer_mappings']['available'],dependencies
            ui=next(item['report'] for item in checked['checks'] if item['name']=='qml_startup')
            assert ui['client_ok'] and ui['client']==dict(workspace_count=0,terminal_count=0,visible_window_count=0,errors=[]),ui
            assert any('libQt6Core.so' in path for item in checked['checks'] if item['name']=='qml_startup' for path in item['report']['loaded_images'])
            after_health=request(state,staged_runtime,home,'health')
            assert before_health['service_pid']==after_health['service_pid'] and before_health['terminals']==after_health['terminals'],'check changed existing runtime state'
            assert before_files=={str(p.relative_to(state)):p.read_bytes() for p in state.rglob('*') if p.is_file() and p.name in ('workspaces.json','qt-view.json')}
            # A clean namespace remains entirely uncreated by --check.
            untouched=root/'check must not create state'
            checked=json.loads(launch('--check',BAND_STATE_DIR=str(untouched),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software').stdout)
            assert checked['ok'] and not untouched.exists(),checked
            display=root/'display.json'
            for desktop in (False,True):
                display.unlink(missing_ok=True)
                with (root/'qt.log').open('w+') as log:
                    client=subprocess.Popen([str(launcher),*(['--desktop'] if desktop else [])],cwd=outside,
                        env=dict(env,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',BAND_DISPLAY_REPORT=str(display)),stdout=log,stderr=log,start_new_session=True)
                    try:
                        def ready():
                            if client.poll() is not None:
                                log.seek(0);raise AssertionError(log.read())
                            try:
                                report=json.loads(display.read_text())
                                return report.get('window_logical_size',[0,0])[0]>0 and any(t.get('ready') for t in report.get('panes',[]))
                            except (OSError,ValueError):return False
                        wait_until(ready,timeout=25)
                        if desktop:
                            assert Path(f'/proc/{client.pid}/exe').resolve()==viewer,'desktop did not replace its launcher with Qt'
                            assert not list((state/'qt').glob('launch-*.json')),'Qt did not retire its temporary configuration'
                            from window_slots import acquire_window_slot
                            lease,view=acquire_window_slot(state/'qt');os.close(lease)
                            assert view.name=='2.view','desktop Qt lost its inherited window lease'
                    finally:
                        if client.poll() is None:os.killpg(client.pid,signal.SIGTERM)
                        client.wait(timeout=10)
                        # A launcher's Qt child shares its lease; let the whole group exit.
                        def group_exited():
                            try:os.killpg(client.pid,0)
                            except ProcessLookupError:return True
                            return False
                        wait_until(group_exited)
                    log.seek(0);output=log.read()
                    if desktop:output+=(state/'launch.log').read_text()
                    assert 'Qt UI: qrc:' not in output,output
            assert any(t['state']=='running' for t in request(state,staged_runtime,home,'health')['terminals']),'viewer exit stopped all shells'
    finally:
        if state.exists():
            data=request(state,Path(paths['runtime']),home)['data']
            ids=[t['id'] for w in data['workspaces'] for t in w['terminals']]
            if ids:request(state,Path(paths['runtime']),home,'session.stop',terminal_ids=ids)
            request(state,Path(paths['runtime']),home,'service.shutdown')
print('PASS: relocated preview, user paths, agent helpers, retained/stopped shells and read-only payload; '+('copied Qt offscreen startup' if sys.platform.startswith('linux') else 'native Mac GUI review pending'))
