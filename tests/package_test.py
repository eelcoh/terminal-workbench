#!/usr/bin/env python3
"""The Linux release archive relocates, uses user state, keeps shells and starts Qt.

Also assembles Band.app's layout with the real Finder launcher compiled for
Linux; that is structural coverage, not a native macOS result.
"""
import importlib.util
import json
import os
import plistlib
import shlex
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path[:0] = [str(ROOT / 'scripts'), str(ROOT / 'tests')]
from session_service import request
from session_smoke import attach, send, wait_until
spec = importlib.util.spec_from_file_location('package', ROOT / 'scripts/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

FAKE_VIEWER = '#!' + sys.executable + '''
import json,os,sys
from pathlib import Path
path=Path(sys.argv[1]);config=json.loads(path.read_text())
if '--remove-launch-config' in sys.argv:path.unlink()
Path(os.environ['WB_PACKAGE_EVIDENCE']).write_text(json.dumps(dict(config=config,pid=os.getpid())))
sys.exit(int(os.environ.get('WB_PACKAGE_EXIT','0')))
'''


def files(root):
    return {str(p.relative_to(root)) for p in root.rglob('*')}


def stop_all(state, runtime, home):
    if (state / 'workspaces.json').exists():
        data = request(state, runtime, home)['data']
        ids = [t['id'] for w in data['workspaces'] for t in w['terminals']]
        if ids:
            request(state, runtime, home, 'session.stop', terminal_ids=ids)
    request(state, runtime, home, 'service.shutdown')


def linux_archive(root):
    archive = package.DIST / f'band-{package.version()}-linux-{package.architecture()}.tar.gz'
    assert archive.is_file(), f'{archive} is missing; run just package'
    with tarfile.open(archive) as source:
        source.extractall(root / 'unpacked', filter='tar')
    extracted = root / 'unpacked' / archive.name.removesuffix('.tar.gz')
    moved = root / 'moved ü $HOME' / 'band'
    moved.parent.mkdir(parents=True)
    extracted.rename(moved)
    return moved


def check_linux(root):
    band = linux_archive(root)
    for path in ('bin/band', 'bin/band-session', 'libexec/band-ui', 'integrations/pi/workbench.ts',
                 'integrations/copilot/hooks.json', 'notices/LICENSE', 'notices/LICENSE.libghostty-vt', 'README.md'):
        assert (band / path).exists(), path
    assert all(os.readlink(band / 'bin' / name) == 'band-session' for name in package.AGENTS)
    info = json.loads((band / 'build-info.json').read_text())
    assert info['version'] == package.version() and len(info['source']) == 40 and not info['qt_bundled']
    home = root / 'home'; home.mkdir(); outside = root / 'outside'; outside.mkdir()
    evidence = root / 'config.json'
    env = {k: v for k, v in os.environ.items() if not k.startswith(('BAND_', 'XDG_', 'WB_PACKAGE_'))}
    env.update(HOME=str(home), SHELL='/bin/sh', WB_PACKAGE_EVIDENCE=str(evidence))
    state = home / '.local/state/band'
    runtime = band / 'bin/band-session'
    viewer = band / 'libexec/band-ui'
    real = viewer.with_name('real-band-ui'); viewer.rename(real)
    viewer.write_text(FAKE_VIEWER); viewer.chmod(0o755)
    # A symlink in PATH must still find the payload.
    linked = root / 'path/band'; linked.parent.mkdir(); linked.symlink_to(band / 'bin/band')

    def launch(*args, exit_code=0, **extra):
        result = subprocess.run([str(linked), *args], cwd=outside, env=dict(env, WB_PACKAGE_EXIT=str(exit_code), **extra),
                                capture_output=True, text=True, timeout=60)
        assert result.returncode == exit_code, (result.stdout, result.stderr)
        return result

    payload = files(band)
    try:
        # A service from another install (here the checkout's debug runtime) is
        # still running. With another window open Band refuses; once it closes,
        # Band hands over and the earlier shell keeps running.
        earlier = request(state, ROOT / 'target/debug/band-session', home)['data']
        first = earlier['workspaces'][0]['terminals'][0]['id']
        earlier_pid = json.loads((state / (first + '.lifecycle')).read_text())['shell_pid']
        earlier_service = request(state, ROOT / 'target/debug/band-session', home, 'health')['service_pid']
        import fcntl
        (state / 'qt/windows').mkdir(parents=True, exist_ok=True)
        with (state / 'qt/windows/7.lock').open('a') as window:
            fcntl.flock(window, fcntl.LOCK_EX)
            refused = launch(exit_code=1)
            assert 'Close every window' in refused.stderr, refused.stderr
        assert request(state, ROOT / 'target/debug/band-session', home, 'health')['service_pid'] == earlier_service
        launch()
        assert request(state, runtime, home, 'health')['service_pid'] != earlier_service
        os.kill(earlier_pid, 0)
        config = json.loads(evidence.read_text())['config']
        assert config['catalog'] == earlier
        assert config['state'] == str(state) and config['cwd'] == str(home) and config['saved_session'] and config['installed']
        assert config['build_info']['version'] == package.version() and config['build_info']['kind'] == 'installed'
        assert config['preferences'] == str(home / '.config/band/config.conf')
        # Shells from the earlier install keep its helpers; new ones get this install's.
        workspace = config['catalog']['workspaces'][0]['id']
        created = request(state, runtime, home, 'terminal.create', workspace, 'Fresh')['data']
        terminal = next(w for w in created['workspaces'] if w['id'] == workspace)['terminals'][-1]['id']
        connection, _, _ = attach(state / (terminal + '.sock'))
        try:
            proof = root / 'shell-proof'
            send(connection, 16, ('printf "%s\\n%s" "$$" "$BAND_AGENT_BIN" > ' + shlex.quote(str(proof)) + '\n').encode())
            wait_until(lambda: proof.exists() and len(proof.read_text().splitlines()) == 2)
            pid, agent_bin = proof.read_text().splitlines()
            assert Path(agent_bin) == band / 'bin'
        finally:
            connection.close()
        service = request(state, runtime, home, 'health')['service_pid']
        launch(exit_code=42)
        os.kill(int(pid), 0)
        assert request(state, runtime, home, 'health')['service_pid'] == service
        # Each agent launcher resolves its integrations after relocation.
        commands = root / 'commands'; commands.mkdir()
        for agent in ('codex', 'claude', 'copilot', 'pi'):
            command = commands / agent
            command.write_text('#!' + sys.executable + '\nimport json,sys\nprint(json.dumps(sys.argv[1:]))\n'); command.chmod(0o755)
            result = subprocess.run([str(band / 'bin' / ('band-' + agent)), 'a spaced argument'], cwd=outside,
                env=dict(env, PATH=str(commands) + os.pathsep + env['PATH'], BAND_AGENT_BIN=agent_bin,
                         BAND_SESSION_SOCKET=str(state / (terminal + '.sock'))), capture_output=True, text=True, check=True)
            arguments = json.loads(result.stdout)
            assert arguments[-1] == 'a spaced argument' and str(ROOT) not in result.stdout
            if agent == 'pi':
                assert arguments[0] == '--extension' and Path(arguments[1]).is_relative_to(band)
            if agent == 'copilot':
                hooks = json.loads((Path(arguments[1]) / 'hooks.json').read_text())
                for entry in hooks['hooks'].values():
                    hook = subprocess.run(entry[0]['bash'], shell=True, input='{}', text=True, capture_output=True,
                                          env=dict(env, BAND_AGENT_BIN=agent_bin), timeout=5)
                    assert hook.returncode == 0 and hook.stdout == hook.stderr == '', (hook.stdout, hook.stderr)
        # Reopening never revives an explicitly stopped terminal.
        request(state, runtime, home, 'session.stop', terminal_ids=[terminal])
        def stopped():
            return next(t['state'] for t in request(state, runtime, home, 'health')['terminals'] if t['id'] == terminal) == 'stopped'
        wait_until(stopped); launch(); assert stopped()
        assert files(band) == payload, 'launching wrote into the release payload'
        # App menu entry: written, refreshed, removed; unknown arguments refused.
        data = root / 'data'
        installed = launch('--install-desktop-entry', XDG_DATA_HOME=str(data))
        entry = data / 'applications/io.github.eelcoh.Band.desktop'
        text = entry.read_text()
        assert 'Name=Band' in text and 'X-Band-Managed=true' in text and str(entry) in installed.stdout
        assert 'Exec="/bin/sh" "' + str(band / 'bin/band').replace('$', '\\\\$') + '"' in text, text
        launch('--install-desktop-entry', '--container', 'build-machine', XDG_DATA_HOME=str(data))
        assert '"distrobox" "enter" "build-machine" "--"' in entry.read_text()
        launch('--remove-desktop-entry', XDG_DATA_HOME=str(data)); assert not entry.exists()
        launch('--bogus', exit_code=2)
        # The real Qt client starts from the archive, plain and as a desktop launch.
        shutil.copy2(real, viewer)
        display = root / 'display.json'
        for desktop in (False, True):
            display.unlink(missing_ok=True)
            with (root / 'qt.log').open('w+') as log:
                client = subprocess.Popen([str(linked), *(['--desktop'] if desktop else [])], cwd=outside,
                    env=dict(env, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', BAND_DISPLAY_REPORT=str(display)),
                    stdout=log, stderr=log, start_new_session=True)
                try:
                    def ready():
                        if client.poll() is not None:
                            log.seek(0); raise AssertionError(log.read())
                        try:
                            report = json.loads(display.read_text())
                            return report.get('window_logical_size', [0, 0])[0] > 0 and any(t.get('ready') for t in report.get('panes', []))
                        except (OSError, ValueError):
                            return False
                    wait_until(ready, timeout=25)
                    if desktop:
                        assert Path(f'/proc/{client.pid}/exe').resolve() == viewer.resolve(), 'desktop launch did not exec Qt'
                finally:
                    if client.poll() is None:
                        os.killpg(client.pid, signal.SIGTERM)
                    client.wait(timeout=10)
                    def group_exited():
                        try: os.killpg(client.pid, 0)
                        except ProcessLookupError: return True
                        return False
                    wait_until(group_exited)
                log.seek(0); output = log.read()
                assert 'Qt UI: qrc:' not in output, output
        if (state / 'launch.log').exists():
            assert 'Band launch:' in (state / 'launch.log').read_text()
    finally:
        if state.exists():
            stop_all(state, runtime, home)


def check_macos_layout(root):
    """Band.app's structure with the real launcher; the native result needs a Mac."""
    headers = root / 'include/mach-o'; headers.mkdir(parents=True)
    (headers / 'dyld.h').write_text('''#include <stdint.h>
#include <unistd.h>
static int _NSGetExecutablePath(char *path,uint32_t *size) {
 ssize_t count=readlink("/proc/self/exe",path,*size-1);
 if(count<0 || count>=(ssize_t)*size-1)return -1;
 path[count]=0;return 0;
}
''')
    launcher = root / 'band-launcher'
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-I', str(headers.parent), str(ROOT / 'native/macos/launcher.c'),
                    '-o', str(launcher)], check=True)
    viewer = root / 'build/band-ui.app/Contents/MacOS/band-ui'
    viewer.parent.mkdir(parents=True); viewer.write_text(FAKE_VIEWER); viewer.chmod(0o755)
    info = viewer.parent.parent / 'Info.plist'
    info.write_bytes(plistlib.dumps(dict(CFBundleExecutable='band-ui', Fixture='kept'))); info.chmod(0o444)
    runtime = ROOT / 'target/release/band-session'
    bundle = package.assemble_macos(root / 'mac', runtime, viewer, launcher, sign=False)
    values = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    assert values['CFBundleExecutable'] == 'Band' and values['CFBundleIdentifier'] == 'io.github.eelcoh.Band'
    assert values['CFBundleName'] == 'Band' and values['Fixture'] == 'kept'
    payload = bundle / 'Contents/Resources/band'
    assert (payload / 'bin/band-session').is_file() and not (payload / 'bin/band').exists()
    assert os.readlink(payload / 'bin/band-codex') == 'band-session'
    home = root / 'mac-home'; home.mkdir()
    evidence = root / 'mac-config.json'
    env = {k: v for k, v in os.environ.items() if not k.startswith(('BAND_', 'XDG_', 'WB_PACKAGE_'))}
    env.update(HOME=str(home), SHELL='/bin/sh', WB_PACKAGE_EVIDENCE=str(evidence))
    state = home / '.local/state/band'
    try:
        result = subprocess.run([str(bundle / 'Contents/MacOS/Band'), '-psn_0_1'], env=env, capture_output=True, text=True, timeout=60)
        log = (state / 'launch.log').read_text() if (state / 'launch.log').exists() else ''
        assert result.returncode == 0, (result.returncode, result.stdout, result.stderr, log)
        config = json.loads(evidence.read_text())['config']
        assert config['state'] == str(state) and config['viewer'] == str((bundle / 'Contents/MacOS/band-ui').resolve())
        assert (state / 'launch.log').read_text().count('Band launch:') == 1
        refused = subprocess.run([str(bundle / 'Contents/MacOS/Band'), '--anything'], env=env, capture_output=True, text=True)
        assert refused.returncode == 2 and 'usage' in refused.stderr
    finally:
        if state.exists():
            stop_all(state, payload / 'bin/band-session', home)


def main():
    with tempfile.TemporaryDirectory(prefix='wb-package-', dir='/tmp') as directory:
        root = Path(directory)
        if not sys.platform.startswith('linux'):
            raise SystemExit('package-test runs on Linux; review Band.app natively on a Mac')
        check_macos_layout(root / 'mac-layout')
        print('PASS: Band.app layout, Info.plist identity and Finder launcher handoff (structural, on Linux)', flush=True)
        check_linux(root / 'linux')
        print('PASS: relocated Linux archive, user state, agent launchers, retained/stopped shells, read-only payload, desktop entry and offscreen Qt start')


if __name__ == '__main__':
    main()
