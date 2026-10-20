"""Verify a newly copied preview without selecting a service or opening a window."""
import json
from datetime import datetime,timezone
import stat
import os
from pathlib import Path
import subprocess
import tempfile

from platform_launchers.macos_tools import run_tool


def verify(directory,launcher,*,timeout=90):
    log=directory/'installation-check.log';raw=directory/'installation-check.stdout'
    receipt=directory/'installation-check.json'
    error=None;result=None
    try:
        with log.open('w') as output,raw.open('w') as stdout:
            os.chmod(log,0o600);os.chmod(raw,0o600)
            run_tool([str(launcher),'--check'],output,'Verifying installed preview',log,timeout=timeout,stdout=stdout)
    except (OSError,ValueError,subprocess.SubprocessError) as failure:error=str(failure)
    try:
        with raw.open('rb') as file:data=file.read(4*1024*1024+1)
        if len(data)>4*1024*1024:raise ValueError('Installed check output exceeds 4 MiB')
        result=json.loads(data)
        if not isinstance(result,dict) or result.get('schema')!=1:raise ValueError('Installed check did not return a supported report')
        checks=result.get('checks')
        if result.get('ok') is not True or not isinstance(checks,list) or not checks or not all(isinstance(row,dict) and row.get('ok') is True for row in checks):
            raise ValueError('Installed preview reported failed or incomplete checks')
        reported=result.get('payload')
        if not isinstance(reported,str) or not Path(reported).is_absolute() or Path(reported).resolve()!=launcher.parent.resolve():
            raise ValueError('Installed check did not identify the copied payload')
    except (OSError,ValueError,RuntimeError) as failure:
        error=(error+'; ' if error else '')+str(failure)
    record=dict(schema=1,ok=error is None,launcher=str(launcher),checked_at=datetime.now(timezone.utc).isoformat(),report=result)
    if error:record['error']=error
    descriptor,temporary=tempfile.mkstemp(prefix='.installation-check-',dir=directory)
    try:
        with os.fdopen(descriptor,'w') as file:json.dump(record,file,indent=2);file.write('\n')
        os.replace(temporary,receipt)
    finally:Path(temporary).unlink(missing_ok=True)
    if error:raise ValueError(f'{error}. Installed copy retained at {directory}. Results: {receipt}. Log: {log}')
    return receipt


def last_check(directory,payload):
    """Read a previous result only; this is not a fresh verification."""
    receipt=directory/'installation-check.json'
    try:
        info=receipt.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid():raise ValueError('Check record must be a regular file owned by you')
        with receipt.open('rb') as file:data=file.read(8*1024*1024+1)
        if len(data)>8*1024*1024:raise ValueError('Check record exceeds 8 MiB')
        value=json.loads(data)
        if not isinstance(value,dict) or value.get('schema')!=1 or type(value.get('ok')) is not bool:
            raise ValueError('Unsupported installation check record')
        if value.get('launcher')!=str(payload/'terminal-workbench'):raise ValueError('Check record belongs to another payload path')
        return dict(result='passed' if value['ok'] else 'failed',checked_at=value.get('checked_at'),file=str(receipt))
    except FileNotFoundError:return dict(result='not-run')
    except (OSError,ValueError) as error:return dict(result='unavailable',error=str(error))
