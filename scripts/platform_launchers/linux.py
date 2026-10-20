"""Manage the opt-in desktop entry after relocation and preview activation."""
import os
import json
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


def desktop_path(environment):
    home=Path(environment.get('HOME',str(Path.home())))
    base=Path(environment.get('XDG_DATA_HOME',str(home/'.local/share'))).expanduser()
    if not base.is_absolute():raise ValueError('XDG_DATA_HOME must be an absolute path')
    return base/'applications'/(IDENTITY+'.desktop')


def install(launcher,environment,container=''):
    path=desktop_path(environment);directory=path.parent;directory.mkdir(parents=True,exist_ok=True)
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


def system_dependency_path(path):
    """Conventional platform library locations, after resolving path aliases."""
    return any(path.is_relative_to(root) for root in ('/usr/lib','/usr/lib64','/lib','/lib64'))


def follow_activation(launcher,state,environment):
    """Follow successful default-session selection only for an existing managed entry."""
    from installed_paths import session_directory
    try:
        default_environment={key:value for key,value in environment.items() if key!='WORKBENCH_STATE_DIR'}
        if state.resolve()!=session_directory(default_environment):
            return dict(result='skipped',reason='custom-session')
        path=desktop_path(environment)
        if path.is_symlink():return dict(result='skipped',reason='unmanaged-entry',path=str(path))
        if not path.exists():return dict(result='skipped',reason='not-registered')
        if not path.is_file():return dict(result='skipped',reason='unmanaged-entry',path=str(path))
        original=path.read_text()
        if MARKER not in original.splitlines():return dict(result='skipped',reason='unmanaged-entry',path=str(path))
        metadata=json.loads((launcher.parent/'preview.json').read_text())
        if not isinstance(metadata,dict):raise ValueError('Invalid preview metadata')
        container=metadata.get('container','')
        if not isinstance(container,str):raise ValueError('Invalid preview container name')
        if original==entry(launcher.resolve(),container):return dict(result='already-current',path=str(path))
        installed=install(launcher,environment,container)
        return dict(result='updated',path=str(installed),launcher=str(launcher.resolve()))
    except (OSError,ValueError,RuntimeError) as error:
        # The service has already been selected. A launcher write failure must
        # not report a failed activation or trigger restoration/replay.
        return dict(result='warning',error=str(error))
