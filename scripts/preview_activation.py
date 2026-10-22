"""Explicit installed-preview service handoff; never stops or recovers a shell."""
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
from session_service import PROTOCOL,Unavailable,exchange,prepare_directory,request,_wait_for_startup
from window_slots import reserve_window_slots


def inspect(state, *, timeout=None):
    if not state.exists():return None
    metadata=state.stat()
    if not stat.S_ISDIR(metadata.st_mode) or metadata.st_uid!=os.getuid() or metadata.st_mode&0o022:
        raise ValueError('Session directory must be owned by you and not writable by other users')
    try:return exchange(state,dict(protocol=PROTOCOL,method='health'),timeout=timeout)
    except Unavailable:return None


def status(state,runtime,cwd):
    health=inspect(state)
    return dict(state_directory=str(state),selected_runtime=str(runtime),service=health,
        activation_required=bool(health and (health.get('runtime_binary')!=str(runtime) or health.get('directory')!=str(cwd))),
        payload_usage=payload_usage(runtime,health))


def _preview(runtime):
    if not isinstance(runtime,(str,Path)):
        raise ValueError('Session service did not identify its staged runtime')
    runtime=Path(runtime)
    if not runtime.is_file() or not os.access(runtime,os.X_OK):
        raise ValueError(f'Preview runtime is missing or not executable: {runtime}')
    path=runtime.parent.parent/'preview.json'
    try:
        metadata=path.lstat()
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid!=os.getuid():
            raise ValueError('Preview metadata must be a regular file owned by you')
        value=json.loads(path.read_text())
        if not isinstance(value,dict) or value.get('format')!=1:raise ValueError('Unknown preview metadata format')
    except (OSError,ValueError) as error:
        raise ValueError(f'Cannot verify a staged preview at {runtime.parent.parent}: {error}') from error


def restore_previous_service(state,previous):
    """Best-effort authority restoration only; never replace a responding service."""
    try:
        current=inspect(state,timeout=5)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        return f'Previous service was not restored because current status could not be verified: {error}'
    if current is not None:
        return 'A responding service was left unchanged; previous service was not restored'
    runtime=Path(previous['runtime_binary']);cwd=Path(previous['directory'])
    try:
        _preview(runtime)
        payload=runtime.parent.parent;script=payload/'scripts/session_service.py'
        if not script.is_file():raise ValueError('Previous preview service script is missing')
        if 'def service_command(' in script.read_text():
            # That payload's runtime binary serves the authority itself.
            command=[str(runtime),'service','--state-dir',str(state),'--runtime',str(runtime),
                '--cwd',str(cwd),'--agent-bin',str(payload/'.local/bin')]
        else:
            # Older payloads predate the Rust service and run their Python server.
            recorded=payload/'python.path'
            python=Path(recorded.read_text().rstrip('\r\n')) if recorded.is_file() else Path(sys.executable)
            if not python.is_absolute() or not python.is_file() or not os.access(python,os.X_OK):
                raise ValueError('Previous preview Python interpreter is missing or not executable')
            command=[str(python),'-B',str(script),'serve','--state-dir',str(state),
                '--runtime',str(runtime),'--cwd',str(cwd)]
        with (state/'service.log').open('ab',buffering=0) as log:
            child=subprocess.Popen(command,stdin=subprocess.DEVNULL,
                stdout=log,stderr=log,start_new_session=True,close_fds=True)
        # Readiness owns cleanup of a failed child. No initialize/recover/mutation
        # request is issued, so journals and stopped terminals stay untouched.
        _wait_for_startup(state,dict(protocol=PROTOCOL,method='health'),child)
        restored=inspect(state,timeout=5)
        if not restored or restored.get('runtime_binary')!=str(runtime) or restored.get('directory')!=str(cwd):
            return 'Another service configuration is responding; it was left unchanged'
        return f'Previous preview service restored from {payload}; reopen that preview to continue'
    except (OSError,ValueError,subprocess.SubprocessError,Unavailable) as error:
        return f'Previous preview service could not be restored: {error}; inspect {state / "service.log"}'


def activate(state,runtime,viewer,cwd):
    _preview(runtime)
    if not viewer.is_file() or not os.access(viewer,os.X_OK):
        raise ValueError('Preview viewer is missing or not executable; rebuild before activating')
    # Exercise only the candidate payload before creating state, taking window
    # reservations, or asking the active service to retire.
    from preview_check import check
    verification=check(runtime.parent.parent,runtime,viewer)
    if not verification['ok']:
        failed=', '.join(item['name'] for item in verification['checks'] if not item['ok'])
        raise ValueError(f'Preview activation checks failed ({failed}); run this preview with --check for details. The current service was left unchanged')
    verification=dict(ok=True,checks=[dict(name=item['name'],ok=item['ok']) for item in verification['checks']])
    from preview_service_check import check as check_service
    verification['checks'].append(check_service(runtime,cwd))
    def selected_result(kind,selected):
        value=dict(result=kind,service=selected,verification=verification)
        if sys.platform.startswith('linux'):
            from platform_launchers.linux import follow_activation
            value['desktop_entry']=follow_activation(runtime.parent.parent/'terminal-workbench',state,os.environ)
        return value
    prepare_directory(state)
    views=state/'qt';views.mkdir(mode=0o700,exist_ok=True)
    with reserve_window_slots(views):
        health=inspect(state)
        if health and health.get('runtime_binary')==str(runtime) and health.get('directory')==str(cwd):
            return selected_result('already-active',health)
        if health:
            if health.get('directory')!=str(cwd):
                raise ValueError('This service uses a different initial directory; its sessions were left unchanged')
            _preview(health.get('runtime_binary',''))
            instance=health.get('service_instance')
            methods=health.get('methods')
            if not isinstance(methods,list) or 'service.retire' not in methods or not isinstance(instance,str) or len(instance)!=32:
                raise ValueError('This older preview service cannot be switched safely. Stop it once using its documented service stop command, then retry; shells can remain running')
            exchange(state,dict(protocol=PROTOCOL,method='service.retire',expected_instance=instance))
        # Health starts only the service, without seeding a missing catalog,
        # finishing journals, attaching PTYs or restarting stopped terminals.
        try:
            selected=request(state,runtime,cwd,'health')
            if selected.get('runtime_binary')!=str(runtime) or selected.get('directory')!=str(cwd):
                raise ValueError('Another service configuration is active; existing shells were left running')
        except (OSError,ValueError,subprocess.SubprocessError,Unavailable) as error:
            if health is None:raise
            restoration=restore_previous_service(state,health)
            raise ValueError(f'Preview activation failed: {error}. {restoration}') from error
        return selected_result('activated' if health else 'started',selected)


def payload_usage(selected_runtime,health):
    """Observed references only; never authorize removal or enumerate old installs."""
    builds={};unknown=[]
    def payload(path,suffix):
        if not isinstance(path,str) or not path or len(path)>4096:return None
        path=Path(path)
        if not path.is_absolute() or tuple(path.parts[-len(suffix):])!=suffix:return None
        return path.parents[len(suffix)-1]
    def reference(directory,kind,identity=None):
        if directory is None:return False
        row=builds.setdefault(str(directory),dict(directory=str(directory),selected=False,service=False,runtime_terminals=[],helper_terminals=[]))
        if identity is None:row[kind]=True
        else:row[kind].append(identity)
        return True
    reference(payload(str(selected_runtime),('libexec','workbench-session')),'selected')
    if health:
        reference(payload(health.get('runtime_binary'),('libexec','workbench-session')),'service')
        for terminal in health.get('terminals',[]):
            if terminal.get('state')=='stopped':continue
            identity=terminal.get('id')
            runtime_info=terminal.get('runtime')
            origin=runtime_info.get('origin') if isinstance(runtime_info,dict) else None
            if terminal.get('state')!='running' or not isinstance(origin,dict) or type(origin.get('version')) is not int or origin['version']!=1:
                unknown.append(dict(id=identity,reason='Runtime origin unavailable'));continue
            runtime=payload(origin.get('runtime_binary'),('libexec','workbench-session'))
            helpers=payload(origin.get('agent_bin'),('.local','bin'))
            reference(runtime,'runtime_terminals',identity)
            reference(helpers,'helper_terminals',identity)
            if runtime is None or helpers is None:unknown.append(dict(id=identity,reason='Runtime or helper path is unavailable or outside a staged preview'))
    return dict(builds=[builds[key] for key in sorted(builds)],unknown_terminals=unknown,
        service_available=health is not None,scope='Selected preview and current service catalog only; other state directories, unlisted runtimes and child-program dependencies are not inspected',
        removal_assessment='Not performed; no files are removed')
