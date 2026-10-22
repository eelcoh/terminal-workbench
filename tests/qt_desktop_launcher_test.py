"""Desktop exec/lease/logging and platform launch contracts, isolated from user files."""
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import shlex
import subprocess
import sys
import tempfile
from types import SimpleNamespace

ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tests')]
from platform_launchers import linux,macos
from session_service import request
from session_smoke import wait_until
from window_slots import acquire_window_slot
spec=importlib.util.spec_from_file_location('stage_toolkit',ROOT/'scripts/stage-toolkit.py')
staging=importlib.util.module_from_spec(spec);spec.loader.exec_module(staging)
runtime=Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory(prefix='wb-desk-',dir='/tmp') as directory:
    root=Path(directory);home=root/'home';home.mkdir()
    env={k:v for k,v in os.environ.items() if not k.startswith(('WORKBENCH_','XDG_'))}
    env.update(HOME=str(home),SHELL='/bin/sh',PYTHONDONTWRITEBYTECODE='1')
    fake=root/'viewer'
    fake.write_text('#!'+sys.executable+'\n'+'''import json,os,sys
from pathlib import Path
path=Path(sys.argv[1]);config=json.loads(path.read_text())
if '--remove-launch-config' in sys.argv:path.unlink()
Path(os.environ['WB_DESKTOP_EVIDENCE']).write_text(json.dumps(dict(config=config,pid=os.getpid(),temporary=str(path))))
print('DESKTOP-VIEWER-STARTED',flush=True)
print('DESKTOP-STDERR',file=sys.stderr,flush=True)
sys.exit(int(os.environ.get('WB_DESKTOP_EXIT','0')))
''');fake.chmod(0o755)
    def cleanup(state,binary):
        if (state/'workspaces.json').exists():
            data=request(state,binary,home)['data']
            ids=[t['id'] for w in data['workspaces'] for t in w['terminals']]
            if ids:request(state,binary,home,'session.stop',terminal_ids=ids)
        request(state,binary,home,'service.shutdown')
    if sys.platform.startswith('linux'):
        # Compile the actual Mac entry point with only its executable-location
        # API substituted. This tests exec/argv behavior, not LaunchServices.
        headers=root/'include/mach-o';headers.mkdir(parents=True)
        (headers/'dyld.h').write_text('''#include <stdint.h>
#include <unistd.h>
static int _NSGetExecutablePath(char *path,uint32_t *size) {
 ssize_t count=readlink("/proc/self/exe",path,*size-1);
 if(count<0 || count>=(ssize_t)*size-1)return -1;
 path[count]=0;return 0;
}
''')
        bootstrap=root/'bootstrap'
        subprocess.run(['cc','-Wall','-Wextra','-Werror','-I',str(headers.parent),str(ROOT/'native/macos/preview_launcher.c'),'-o',str(bootstrap)],check=True)
    else:bootstrap=ROOT/'.local/build/qt-input/workbench-preview-bootstrap'
    source_app=root/'source.app/Contents/MacOS/workbench-ui-study'
    source_app.parent.mkdir(parents=True);shutil.copy2(fake,source_app)
    # CMake's real Mac Info.plist can be read-only; copytree preserves its mode.
    source_info=source_app.parent.parent/'Info.plist'
    original_info=plistlib.dumps(dict(CFBundleExecutable='workbench-ui-study',WorkbenchFixture='preserved'))
    source_info.write_bytes(original_info);source_info.chmod(0o444)
    original=staging.sys
    try:
        staging.sys=SimpleNamespace(platform='darwin',executable=sys.executable)
        staged=staging.stage(root/'mac',runtime,source_app,bootstrap,sign=False)
    finally:staging.sys=original
    # Move the .app alone, including its entire payload.
    bundle=root/'Moved app.app';(staged/macos.NAME).rename(bundle);shutil.rmtree(staged)
    payload=bundle/'Contents/Resources/workbench'
    info=plistlib.loads((bundle/'Contents/Info.plist').read_bytes())
    assert info['CFBundleName']=='Terminal Workbench Preview' and info['CFBundlePackageType']=='APPL'
    assert info['WorkbenchFixture']=='preserved'
    assert source_info.read_bytes()==original_info and source_info.stat().st_mode&0o777==0o444,'staging modified the source bundle metadata'
    assert not list((bundle/'Contents').glob('.workbench-info-*')),'staging left temporary bundle metadata'
    entry=bundle/'Contents/MacOS'/info['CFBundleExecutable']
    state=root/'mac-state';binary=payload/'libexec/workbench-session';evidence=root/'mac.json'
    context=dict(env,PATH='/usr/bin:/bin',WORKBENCH_STATE_DIR=str(state),WB_DESKTOP_EVIDENCE=str(evidence))
    # Management arguments still find the recorded Python with a Finder-like PATH.
    paths=subprocess.run([str(entry),'--paths'],env=context,capture_output=True,text=True,check=True)
    assert json.loads(paths.stdout)['runtime']==str(binary)
    assert not state.exists(),'--paths started a service/log'
    try:
        with subprocess.Popen([str(entry)],env=context,stdout=subprocess.PIPE,stderr=subprocess.PIPE) as process:
            stdout,stderr=process.communicate(timeout=45)
            assert process.returncode==0,(stdout,stderr)
            capture=json.loads(evidence.read_text());assert capture['pid']==process.pid,'bootstrap left a launcher process in front of Qt'
        assert capture['config']['installed_preview'] and not Path(capture['temporary']).exists()
        lease,view=acquire_window_slot(state/'qt');os.close(lease)
        assert view.name=='1.view','window lease survived viewer exit'
        text=(state/'launch.log').read_text()
        assert 'DESKTOP-VIEWER-STARTED' in text and 'DESKTOP-STDERR' in text
        assert (state/'launch.log').stat().st_mode&0o077==0
        assert all(t['state']=='running' for t in request(state,binary,home,'health')['terminals'])
        if sys.platform.startswith('linux'):
            # Failure coverage without triggering a real desktop notification.
            viewer=bundle/'Contents/MacOS/workbench-ui-study'
            viewer.rename(viewer.with_name('hidden-viewer'))
            failed=subprocess.run([str(entry)],env=dict(context,PATH='/nonexistent'),capture_output=True,text=True,timeout=45)
            assert failed.returncode==1 and 'No such file or directory' in (state/'launch.log').read_text()
            assert not list((state/'qt').glob('launch-*.json'))
            lease,view=acquire_window_slot(state/'qt');os.close(lease);assert view.name=='1.view'
            assert all(t['state']=='running' for t in request(state,binary,home,'health')['terminals'])
            victim=root/'victim';victim.write_text('retained')
            bad_state=root/'bad-state';bad_state.mkdir();(bad_state/'launch.log').symlink_to(victim)
            failed=subprocess.run([str(entry)],env=dict(context,PATH='/nonexistent',WORKBENCH_STATE_DIR=str(bad_state)),capture_output=True,text=True,timeout=15)
            assert failed.returncode==1 and victim.read_text()=='retained'
            assert not (bad_state/'workspaces.json').exists(),'log failure started sessions'
    finally:cleanup(state,binary)
    if sys.platform.startswith('linux'):
        # Exercise desktop Exec decoding with GLib, including literal reserved
        # characters and Unicode. A fake distrobox proves context/argv routing.
        special=root/'space " $ ` % \\ ü';special.mkdir()
        launcher=special/'terminal-workbench'
        output=root/'argv.json'
        code='import json,os,sys;from pathlib import Path;Path(os.environ["WB_DESKTOP_ARGV"]).write_text(json.dumps(sys.argv[1:]))'
        launcher.write_text('#!/bin/sh\nexec '+shlex.quote(sys.executable)+' -c '+shlex.quote(code)+' "$0" "$@"\n');launcher.chmod(0o755)
        desktop_env=dict(env,WB_DESKTOP_ARGV=str(output),XDG_DATA_HOME=str(root/'data'))
        installed=linux.install(launcher,desktop_env)
        subprocess.run(['desktop-file-validate',str(installed)],check=True)
        result=subprocess.run(['gio','launch',str(installed)],env=desktop_env,capture_output=True,text=True)
        assert result.returncode==0,(result.stdout,result.stderr,installed.read_text())
        wait_until(output.exists)
        assert json.loads(output.read_text())==[str(launcher),'--desktop']
        output.unlink()
        commands=root/'commands';commands.mkdir()
        distrobox=commands/'distrobox'
        distrobox.write_text('#!'+sys.executable+'\nimport json,os,sys\nfrom pathlib import Path\nPath(os.environ["WB_DESKTOP_ARGV"]).write_text(json.dumps(sys.argv[1:]))\n');distrobox.chmod(0o755)
        linux.install(launcher,desktop_env,'build machine')
        subprocess.run(['desktop-file-validate',str(installed)],check=True)
        subprocess.run(['gio','launch',str(installed)],env=dict(desktop_env,PATH=str(commands)+os.pathsep+env['PATH']),check=True,capture_output=True)
        wait_until(output.exists)
        assert json.loads(output.read_text())==['enter','build machine','--',str(launcher),'--desktop']
        installed.write_text('[Desktop Entry]\nType=Application\nName=Other application\n')
        original=installed.read_bytes()
        try:linux.install(launcher,desktop_env)
        except ValueError:pass
        else:raise AssertionError('unmanaged desktop entry overwritten')
        assert installed.read_bytes()==original
        installed.unlink();installed.symlink_to(output)
        try:linux.install(launcher,desktop_env)
        except ValueError:pass
        else:raise AssertionError('desktop entry symlink overwritten')
print('PASS: standalone Mac bundle layout, native bootstrap exec/argv with minimal PATH, private launch logs, released leases and retained shells; '+('GLib desktop escaping/Distrobox routing and protected user entries' if sys.platform.startswith('linux') else 'Finder/signing acceptance pending'))
