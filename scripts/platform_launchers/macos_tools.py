"""Bounded, observable Mac packaging tools; cancel their subprocess groups too."""
import os
import signal
import subprocess
import time


def progress(message):
    print(message,flush=True)


def _stop(process):
    try:os.killpg(process.pid,signal.SIGTERM)
    except ProcessLookupError:pass
    try:process.wait(timeout=1)
    except subprocess.TimeoutExpired:pass
    finally:
        # The leader can exit while a scanner/signing child ignores SIGTERM.
        try:os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError:pass
        process.wait()


def _descendants(text,root):
    rows={}
    for line in text.splitlines():
        columns=line.strip().split(None,4)
        if len(columns)!=5:continue
        try:pid,parent=int(columns[0]),int(columns[1])
        except ValueError:continue
        rows[pid]=(parent,line.strip())
    selected={root}
    while True:
        additional={pid for pid,(parent,_) in rows.items() if parent in selected}-selected
        if not additional:break
        selected.update(additional)
    return [rows[pid][1] for pid in sorted(selected) if pid in rows]


def _snapshot(output,pid,elapsed):
    output.write(f'\nPackaging process snapshot after {elapsed:.0f}s (PID PPID STATE ELAPSED COMMAND):\n')
    try:
        reply=subprocess.run(['/bin/ps','-axo','pid=,ppid=,stat=,etime=,comm='],
            check=True,capture_output=True,text=True,errors='replace',timeout=2)
        for row in _descendants(reply.stdout,pid):output.write(row+'\n')
    except (OSError,subprocess.SubprocessError) as error:
        output.write(f'Process snapshot unavailable: {type(error).__name__}\n')
    output.flush()


def run_tool(arguments,output,label,log_path,*,timeout=300,heartbeat=5,report=progress,cwd=None,env=None,stdout=None):
    report(f'{label} (limit {timeout:g}s). Log: {log_path}')
    started=time.monotonic()
    process=subprocess.Popen(arguments,stdin=subprocess.DEVNULL,stdout=output if stdout is None else stdout,
        stderr=subprocess.STDOUT if stdout is None else output,start_new_session=True,cwd=cwd,env=env)
    try:
        output.write(f'{label}: PID {process.pid}, timeout {timeout:g}s\n');output.flush()
        while True:
            remaining=timeout-(time.monotonic()-started)
            if remaining<=0:raise subprocess.TimeoutExpired(arguments,timeout)
            try:
                code=process.wait(timeout=min(heartbeat,remaining))
                break
            except subprocess.TimeoutExpired:
                _snapshot(output,process.pid,time.monotonic()-started)
                report(f'{label}: still running after {time.monotonic()-started:.0f}s (PID {process.pid}). Log: {log_path}')
        if code:raise subprocess.CalledProcessError(code,arguments)
    except BaseException:
        try:_snapshot(output,process.pid,time.monotonic()-started)
        finally:_stop(process)
        raise
    report(f'{label}: finished in {time.monotonic()-started:.1f}s')
