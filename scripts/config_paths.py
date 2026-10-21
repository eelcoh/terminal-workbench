"""Portable preference paths/migration; no session/runtime ownership is moved."""
from pathlib import Path
import argparse
import os
import sys
import tempfile

EXAMPLE = '''workbench-config-v1
# Managed by you. UI edits go to ui/<platform>/ and never rewrite this file.
# Values are unquoted; # starts a comment only at the beginning of a line.
[appearance]
application-theme = Workbench Night
terminal-theme = Workbench Night
# Optional fixed chrome; workspace selection otherwise previews matching chrome.
# follow-workspace = false
# A theme can also be a Ghostty color file relative to this config file:
# terminal-theme = themes/my-theme

# Qt candidate: values affect future creations; existing sessions/layouts stay saved.
# [workspace]
# directory = ~/Code
# terminal-width = 1/2
# [macos.workspace]
# terminal-width = 2/3

[font]
family = JetBrains Mono
size = 16
# Also bundled: Fira Code. Sizes are logical points, from 10 to 32.
# [macos.font] and [linux.font] can override either field.

[shortcuts]
# Shared bindings; omitted actions retain platform defaults.
# forward = F8

[linux.shortcuts]
# commands = Super+Alt+P

[macos.shortcuts]
# commands = Command+Option+P
'''

def config_directory(environment, platform):
    override = environment.get('WORKBENCH_CONFIG_DIR')
    if override:
        return Path(override).expanduser().absolute()
    home = Path(environment.get('HOME', str(Path.home())))
    return Path(environment.get('XDG_CONFIG_HOME') or home / '.config') / 'terminal-workbench'

def publish_once(destination, data):
    fd, temporary = tempfile.mkstemp(prefix='.migrate-', dir=destination.parent)
    try:
        with os.fdopen(fd, 'wb') as file:
            file.write(data); file.flush(); os.fsync(file.fileno())
        try:
            os.link(temporary, destination)
        except FileExistsError:
            pass
    finally:
        os.unlink(temporary)

def copy_once(source, destination):
    if source.exists() and not destination.exists():
        publish_once(destination, source.read_bytes())

def _copy_missing(source, destination):
    # Never follow a destination symlink or overwrite an existing choice.
    if destination.is_symlink():
        return
    if source.is_symlink():
        if not destination.exists():
            destination.parent.mkdir(parents=True, exist_ok=True)
            try:
                destination.symlink_to(source.resolve(strict=False), target_is_directory=source.is_dir())
            except FileExistsError:
                pass
    elif source.is_dir():
        if destination.exists() and not destination.is_dir():
            return
        destination.mkdir(parents=True, exist_ok=True, mode=0o700)
        for child in sorted(source.iterdir()):
            if not child.name.startswith('.') and not child.name.endswith('.lock'):
                _copy_missing(child, destination / child.name)
    elif source.is_file() and not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        publish_once(destination, source.read_bytes())


def prepare_config_directory(environment, platform):
    directory = config_directory(environment, platform)
    if platform != 'darwin' or environment.get('WORKBENCH_CONFIG_DIR'):
        return directory
    home = Path(environment.get('HOME', str(Path.home())))
    legacy = home / 'Library/Application Support/Terminal Workbench'
    marker = directory / '.migrated-macos-config-v1'
    if marker.exists() or not legacy.is_dir() or directory.resolve() == legacy.resolve():
        return directory
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    _copy_missing(legacy / 'config.conf', directory / 'config.conf')
    _copy_missing(legacy / 'themes', directory / 'themes')
    for name in ('shortcuts.conf', 'appearance.state', 'appearance.conf', 'terminal-font.json', '.migrated-v1'):
        _copy_missing(legacy / 'ui/macos' / name, directory / 'ui/macos' / name)
    publish_once(marker, b'Application Support preferences copied; original files retained.\n')
    return directory


def global_appearance(data):
    # Retain only global sections; scoped identities remain with local sessions.
    try:
        text = data.decode('utf-8')
    except UnicodeError:
        return data
    if not text.startswith('workbench-appearance-v1\n'):
        return data  # Invalid input is preserved and reported by the viewer.
    lines = []
    keep = True
    for line in text.splitlines(keepends=True):
        if line.startswith('@'):
            keep = not (line.startswith('@workspace\t') or line.startswith('@terminal\t'))
        if keep:
            lines.append(line)
    return ''.join(lines).encode()

def prepare_preferences(root, sessions, environment, platform):
    directory = prepare_config_directory(environment, platform)
    system = 'macos' if platform == 'darwin' else 'linux'
    ui = directory / 'ui' / system
    ui.mkdir(parents=True, exist_ok=True, mode=0o700)
    marker = ui / '.migrated-v1'
    if not marker.exists():
        if 'WORKBENCH_SHORTCUTS' not in environment:
            source = root / '.local' / f'shortcuts-{system}.conf'
            if source.exists() and not (ui / 'shortcuts.conf').exists():
                data = source.read_bytes()
                if data.startswith(b'workbench-shortcuts-v1\n'):
                    data = data.replace(b'workbench-shortcuts-v1\n', b'workbench-shortcuts-migrated-v1\n', 1)
                publish_once(ui / 'shortcuts.conf', data)
        source = sessions / 'appearance.state'
        if source.exists() and not (ui / 'appearance.state').exists():
            data = global_appearance(source.read_bytes())
            publish_once(ui / 'appearance.state', data)
        copy_once(sessions / 'appearance.conf', ui / 'appearance.conf')
        try:
            fd = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        except FileExistsError:
            pass
        else:
            os.close(fd)
    values = dict(WORKBENCH_CONFIG=str(directory / 'config.conf'),
                  WORKBENCH_UI_APPEARANCE=str(ui / 'appearance.state'))
    if 'WORKBENCH_SHORTCUTS' not in environment:
        values['WORKBENCH_SHORTCUTS'] = str(ui / 'shortcuts.conf')
        values['WORKBENCH_LAYERED_SHORTCUTS'] = '1'
    return values

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--init', action='store_true')
    args = parser.parse_args()
    directory = prepare_config_directory(os.environ, sys.platform) if args.init else config_directory(os.environ, sys.platform)
    path = directory / 'config.conf'
    if args.init:
        path.parent.mkdir(parents=True, exist_ok=True)
        try:
            with path.open('x') as file:
                file.write(EXAMPLE)
        except FileExistsError:
            print(f'Existing configuration preserved: {path}')
            return
    print(path)

if __name__ == '__main__':
    main()
