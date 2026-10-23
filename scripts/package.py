#!/usr/bin/env python3
"""Build the Band release artifact for this platform.

macOS: dist/Band-<version>-macos-<arch>.zip holding Band.app with Qt deployed
and ad-hoc signed (not notarized). Linux: dist/band-<version>-linux-<arch>.tar.gz
using the system Qt 6.8+. Neither needs Python at run time.
"""
import argparse
import json
import os
import platform
import plistlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / '.local/build/release'
DIST = ROOT / 'dist'
APP_ID = 'io.github.eelcoh.Band'
AGENTS = ('band-codex', 'band-claude', 'band-copilot', 'band-pi', 'band-agent-hook')
LAUNCHER = '''#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$(readlink -f -- "$0")")/.." && pwd)
case "${1-}" in
    "") exec "$root/bin/band-session" open --payload "$root" --viewer "$root/libexec/band-ui" ;;
    --desktop) exec "$root/bin/band-session" open --payload "$root" --viewer "$root/libexec/band-ui" --desktop ;;
    --install-desktop-entry) shift; exec "$root/bin/band-session" desktop-entry install "$@" ;;
    --remove-desktop-entry) exec "$root/bin/band-session" desktop-entry remove ;;
    *) echo "usage: band [--install-desktop-entry [--container NAME] | --remove-desktop-entry]" >&2; exit 2 ;;
esac
'''


def version():
    return tomllib.loads((ROOT / 'Cargo.toml').read_text())['workspace']['package']['version']


def architecture():
    machine = platform.machine().lower()
    return {'amd64': 'x86_64', 'aarch64': 'arm64'}.get(machine, machine)


def build_info(*, qt_bundled):
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    dirty = bool(subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT, capture_output=True, text=True, check=True).stdout)
    return dict(format=1, version=version(), source=revision, source_dirty=dirty, platform=sys.platform,
                architecture=architecture(), qt_bundled=qt_bundled)


def payload_files(payload, runtime, *, qt_bundled):
    """Runtime, agent launchers, integrations, notices and build identity."""
    binaries = payload / 'bin'
    binaries.mkdir(parents=True)
    shutil.copy2(runtime, binaries / 'band-session')
    # The runtime acts as each launcher, selected by the name it is invoked through.
    for name in AGENTS:
        (binaries / name).symlink_to('band-session')
    # Agent hooks find integrations beside bin/ (BAND_AGENT_BIN/../integrations).
    for name in ('pi', 'copilot'):
        shutil.copytree(ROOT / '.local/integrations' / name, payload / 'integrations' / name)
    notices = payload / 'notices'
    notices.mkdir()
    shutil.copy2(ROOT / 'LICENSE', notices / 'LICENSE')
    shutil.copy2(ROOT / 'crates/runtime/native/LICENSE.libghostty-vt', notices / 'LICENSE.libghostty-vt')
    for name in ('fonts', 'themes'):
        for path in (ROOT / name).rglob('*'):
            if path.is_file() and path.name in ('LICENSE', 'OFL.txt', 'README.md'):
                destination = notices / path.relative_to(ROOT)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, destination)
    (payload / 'build-info.json').write_text(json.dumps(build_info(qt_bundled=qt_bundled), indent=2) + '\n')


def assemble_linux(parent, runtime, viewer):
    root = parent / f'band-{version()}-linux-{architecture()}'
    root.mkdir(parents=True)
    payload_files(root, runtime, qt_bundled=False)
    (root / 'libexec').mkdir()
    shutil.copy2(viewer, root / 'libexec/band-ui')
    (root / 'bin/band').write_text(LAUNCHER)
    (root / 'bin/band').chmod(0o755)
    shutil.copy2(ROOT / 'docs/packaging.md', root / 'README.md')
    return root


def assemble_macos(parent, runtime, viewer, launcher, *, deploy=None, sign=True):
    app = viewer.parents[2]
    if app.suffix != '.app' or viewer.name != 'band-ui':
        raise ValueError('Expected the band-ui macOS application bundle')
    bundle = parent / 'Band.app'
    shutil.copytree(app, bundle, symlinks=True)
    payload = bundle / 'Contents/Resources/band'
    payload_files(payload, runtime, qt_bundled=deploy is not None)
    shutil.copy2(launcher, bundle / 'Contents/MacOS/Band')
    info = bundle / 'Contents/Info.plist'
    values = plistlib.loads(info.read_bytes()) if info.exists() else {}
    values.update(CFBundleExecutable='Band', CFBundleIdentifier=APP_ID, CFBundleName='Band',
                  CFBundleDisplayName='Band', CFBundlePackageType='APPL', CFBundleVersion=version(),
                  CFBundleShortVersionString=version(), NSHighResolutionCapable=True)
    # CMake may generate a read-only plist; publish a new file instead.
    info.unlink(missing_ok=True)
    info.write_bytes(plistlib.dumps(values))
    if deploy is not None:
        deploy(bundle, payload)
    if sign:
        subprocess.run(['/usr/bin/codesign', '--force', '--deep', '--sign', '-', str(bundle)], check=True)
    return bundle


def archive_linux(root, destination):
    with tarfile.open(destination, 'w:gz') as archive:
        archive.add(root, arcname=root.name)


def archive_macos(bundle, destination):
    subprocess.run(['/usr/bin/ditto', '-c', '-k', '--keepParent', str(bundle), str(destination)], check=True)


def build():
    subprocess.run([sys.executable, str(ROOT / 'scripts/build-runtime.py'), '--release'], check=True)
    subprocess.run(['cmake', '-S', str(ROOT / 'spikes/qt-input'), '-B', str(BUILD),
                    '-DCMAKE_BUILD_TYPE=Release', '-DBAND_TERMINAL_PROBE=ON'], check=True)
    targets = ['band-ui', *(['band-launcher'] if sys.platform == 'darwin' else [])]
    subprocess.run(['cmake', '--build', str(BUILD), '--parallel', '--target', *targets], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--no-build', action='store_true', help='Package the existing release build')
    args = parser.parse_args()
    if sys.platform != 'darwin' and not sys.platform.startswith('linux'):
        parser.error('Packaging supports macOS and Linux')
    if not args.no_build:
        build()
    runtime = ROOT / 'target/release/band-session'
    DIST.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='package-', dir=DIST) as directory:
        parent = Path(directory)
        if sys.platform == 'darwin':
            sys.path.insert(0, str(ROOT / 'scripts'))
            from platform_launchers.macos_deploy import toolchain, deploy
            info = toolchain(BUILD / 'workbench-qt-tools.txt')
            bundle = assemble_macos(parent, runtime, BUILD / 'band-ui.app/Contents/MacOS/band-ui', BUILD / 'band-launcher',
                                    deploy=lambda bundle, payload: deploy(bundle, payload, info, ROOT / 'native/macos/qt-deployment'))
            destination = DIST / f'Band-{version()}-macos-{architecture()}.zip'
            destination.unlink(missing_ok=True)
            archive_macos(bundle, destination)
        else:
            root = assemble_linux(parent, runtime, BUILD / 'band-ui')
            destination = DIST / f'{root.name}.tar.gz'
            archive_linux(root, destination)
    print(f'Release artifact: {destination}')


if __name__ == '__main__':
    main()
