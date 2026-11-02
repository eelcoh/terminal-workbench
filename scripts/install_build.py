#!/usr/bin/env python3
"""Install a Band build for this platform.

macOS: Band.app into ~/Applications. Linux: the archive into ~/.local/opt.
Installs a local `just package` archive, an artifact zip downloaded in a
browser (its quarantine flag is cleared, so Gatekeeper does not stop the ad-hoc
signed app), or by default the newest green CI build, fetched with `gh`. Development tooling; runs on the system Python 3.9 on macOS.
"""
import argparse
import json
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGETS = {
    'Darwin': ('macos.yml', 'Band-macos-arm64', Path.home() / 'Applications'),
    'Linux': ('linux.yml', 'band-linux-x86_64', Path.home() / '.local/opt'),
}


def gh(*args):
    return subprocess.run(('gh',) + args, cwd=ROOT, check=True, text=True,
                          stdout=subprocess.PIPE).stdout


def latest_run(workflow, branch):
    runs = json.loads(gh('run', 'list', '--workflow', workflow, '--branch', branch,
                         '--status', 'success', '--limit', '1',
                         '--json', 'databaseId,headSha,createdAt'))
    if not runs:
        sys.exit(f'no successful {workflow} run on {branch}')
    return runs[0]


def unpack_artifact(path, download):
    # A browser saves the artifact as a zip around the release archive (Safari
    # may unpack it to a folder); accept that or the release archive itself.
    if path.is_dir():
        for item in path.iterdir():
            if item.is_file():
                shutil.copy2(item, download)
    elif zipfile.is_zipfile(path) and not any(
            name.startswith('Band.app/') for name in zipfile.ZipFile(path).namelist()):
        with zipfile.ZipFile(path) as artifact:
            for name in artifact.namelist():
                if '/' in name or name.startswith('.'):
                    sys.exit(f'unexpected path in artifact: {name}')
            artifact.extractall(download)
    else:
        shutil.copy2(path, download)


def running_from(path):
    # The session service keeps running from the old install; shells survive
    # the replacement, but new ones should start from a fresh window.
    found = subprocess.run(('pgrep', '-f', str(path)), stdout=subprocess.PIPE, text=True)
    return found.returncode == 0


def install_macos(download, dest):
    archives = sorted(download.glob('Band-*-macos-*.zip'))
    if len(archives) != 1:
        sys.exit(f'expected one Band zip in the artifact, found {len(archives)}')
    dest.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix='.band-install-', dir=dest))
    try:
        # ditto keeps the bundle's symlinks, modes and signature intact.
        subprocess.run(('ditto', '-x', '-k', str(archives[0]), str(staging)), check=True)
        app, target = staging / 'Band.app', dest / 'Band.app'
        if not app.is_dir():
            sys.exit('the artifact holds no Band.app')
        if running_from(target):
            print(f'note: Band from {target} is running; its shells keep running.')
        if target.exists():
            shutil.rmtree(target)
        app.rename(target)
    finally:
        shutil.rmtree(staging, ignore_errors=True)
    subprocess.run(('xattr', '-dr', 'com.apple.quarantine', str(target)), check=True)
    print(f'installed {target}')


def install_linux(download, dest):
    archives = sorted(download.glob('band-*-linux-*.tar.gz'))
    if len(archives) != 1:
        sys.exit(f'expected one band archive in the artifact, found {len(archives)}')
    name = archives[0].name[:-len('.tar.gz')]
    dest.mkdir(parents=True, exist_ok=True)
    target = dest / name
    if target.exists():
        shutil.rmtree(target)
    with tarfile.open(archives[0]) as archive:
        for member in archive.getmembers():
            if not member.name == name and not member.name.startswith(name + '/'):
                sys.exit(f'unexpected path in archive: {member.name}')
        archive.extractall(dest)
    print(f'installed {target}')
    print(f'add it to the app menu: {target}/bin/band --install-desktop-entry')


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('artifact', nargs='?', type=Path,
                        help='downloaded artifact zip or release archive (default: fetch with gh)')
    parser.add_argument('--branch', default='main')
    parser.add_argument('--dest', type=Path, help='install folder (default per platform)')
    args = parser.parse_args()
    system = platform.system()
    if system not in TARGETS:
        sys.exit(f'no CI build for {system}')
    workflow, artifact, dest = TARGETS[system]
    with tempfile.TemporaryDirectory() as tmp:
        if args.artifact:
            unpack_artifact(args.artifact.expanduser(), tmp)
        else:
            run = latest_run(workflow, args.branch)
            print(f"run {run['databaseId']}: {run['headSha'][:7]} from {run['createdAt']}")
            gh('run', 'download', str(run['databaseId']), '-n', artifact, '-D', tmp)
        (install_macos if system == 'Darwin' else install_linux)(Path(tmp), args.dest or dest)


if __name__ == '__main__':
    main()
