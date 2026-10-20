"""Rehearse one candidate service in an empty private namespace, without shells."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from session_service import PROTOCOL,Unavailable,exchange,stop_owned_startup


def check(runtime,cwd,*,timeout=5):
    runtime=runtime.resolve();cwd=cwd.resolve()
    script=runtime.parent.parent/'scripts/session_service.py'
    descriptor,log=tempfile.mkstemp(prefix='workbench-service-check-',suffix='.log',dir='/tmp')
    log=Path(log)
    try:
        with os.fdopen(descriptor,'wb',buffering=0) as output:
            # Keep Unix socket paths short on both platforms, including Mac's
            # otherwise lengthy per-user TMPDIR. The directory is mode 0700.
            with tempfile.TemporaryDirectory(prefix='wb-service-',dir='/tmp') as directory:
                state=Path(directory).resolve()
                process=subprocess.Popen([sys.executable,'-B',str(script),'serve','--state-dir',str(state),
                    '--runtime',str(runtime),'--cwd',str(cwd)],stdin=subprocess.DEVNULL,
                    stdout=output,stderr=output,start_new_session=True,close_fds=True)
                deadline=time.monotonic()+timeout
                try:
                    while True:
                        remaining=deadline-time.monotonic()
                        if process.poll() is not None:
                            raise ValueError(f'Candidate session service exited {process.returncode} during startup')
                        if remaining<=0:raise ValueError('Candidate session service startup timed out')
                        try:
                            health=exchange(state,dict(protocol=PROTOCOL,method='health'),timeout=min(remaining,.25))
                            break
                        except (Unavailable,socket.timeout):time.sleep(min(.02,max(0,deadline-time.monotonic())))
                    if (health.get('service_pid')!=process.pid or health.get('runtime_binary')!=str(runtime)
                        or health.get('directory')!=str(cwd) or health.get('terminals')!=[]):
                        raise ValueError('Candidate service returned unexpected startup health')
                    if any(state.glob('*.sock')) or any((state/name).exists() for name in ('workspaces.json','workspaces.tsv')):
                        raise ValueError('Candidate service seeded terminal state during health check')
                    instance=health.get('service_instance');methods=health.get('methods')
                    if not isinstance(instance,str) or len(instance)!=32 or not isinstance(methods,list) or 'service.retire' not in methods:
                        raise ValueError('Candidate service does not support identified retirement')
                    exchange(state,dict(protocol=PROTOCOL,method='service.retire',expected_instance=instance),timeout=1)
                    process.wait(timeout=1)
                    if process.returncode!=0:raise ValueError(f'Candidate service retirement exited {process.returncode}')
                    result=dict(name='service_startup',ok=True,terminal_count=0)
                finally:stop_owned_startup(process)
        log.unlink()
        return result
    except (OSError,ValueError,subprocess.SubprocessError,Unavailable) as error:
        raise ValueError(f'Candidate session service check failed: {error}. Current service was left unchanged. Log: {log}') from error
