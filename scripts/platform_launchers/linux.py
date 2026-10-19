"""Install a user desktop entry only on explicit request, after relocation."""
import os
import shutil
import subprocess
from pathlib import Path
import tempfile

IDENTITY='terminal-workbench-qt-preview'
MARKER='X-TerminalWorkbench-Preview=true'


def exec_argument(value):
    value=str(value)
    if any(ord(ch)<32 or ord(ch)==127 for ch in value):
        raise ValueError('Desktop launch paths cannot contain control characters')
    # Desktop Exec quoting, followed by desktop string-value escaping. This is
    # not a shell command; %% is a literal percent for desktop field expansion.
    value=value.replace('%','%%')
    for character in ('\\','"','`','$'):
        value=value.replace(character,'\\'+character)
    return '"'+value.replace('\\','\\\\')+'"'


def entry(launcher,container=''):
    # Keep the Exec program itself fixed: GLib resolves it before expanding %%
    # in path arguments. Pass the script as an argument, never through sh -c.
    command=['distrobox','enter',container,'--'] if container else ['/bin/sh']
    command += [str(launcher),'--desktop']
    return '\n'.join(('[Desktop Entry]','Type=Application','Version=1.0',
        'Name=Terminal Workbench Preview','Comment=Qt preview with saved terminal sessions',
        'Exec='+' '.join(exec_argument(value) for value in command),
        'TryExec='+('distrobox' if container else str(launcher).replace('\\','\\\\')),
        'Icon=utilities-terminal','Terminal=false','Categories=System;TerminalEmulator;',
        'StartupWMClass='+IDENTITY,'StartupNotify=false',MARKER,''))


def install(launcher,environment,container=''):
    home=Path(environment.get('HOME',str(Path.home())))
    base=Path(environment.get('XDG_DATA_HOME',str(home/'.local/share'))).expanduser()
    if not base.is_absolute():raise ValueError('XDG_DATA_HOME must be an absolute path')
    directory=base/'applications';directory.mkdir(parents=True,exist_ok=True)
    path=directory/(IDENTITY+'.desktop')
    if path.is_symlink() or (path.exists() and MARKER not in path.read_text().splitlines()):
        raise ValueError(f'Refusing to replace an unmanaged desktop entry: {path}')
    contents=entry(launcher.resolve(),container)
    descriptor,temporary=tempfile.mkstemp(prefix=IDENTITY+'-',suffix='.desktop',dir=directory)
    try:
        with os.fdopen(descriptor,'w') as file:file.write(contents)
        os.chmod(temporary,0o644);os.replace(temporary,path)
    finally:
        Path(temporary).unlink(missing_ok=True)
    return path


def show_error(message):
    command=shutil.which('notify-send')
    if command:
        try:subprocess.run([command,'--app-name=Terminal Workbench Preview','Terminal Workbench could not start',message],check=False,timeout=5)
        except (OSError,subprocess.TimeoutExpired):pass
