"""Desktop-only logging and exec handoff, with no change to PTY ownership."""
from datetime import datetime,timezone
import os
from pathlib import Path
import stat
import sys
from session_service import prepare_directory


def start_log(state):
    prepare_directory(state)
    path=state/'launch.log'
    descriptor=os.open(path,os.O_WRONLY|os.O_APPEND|os.O_CREAT|os.O_NOFOLLOW,0o600)
    try:
        metadata=os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid!=os.getuid() or metadata.st_mode&0o077:
            raise ValueError('Launch log must be a private regular file owned by you')
        sys.stdout.flush();sys.stderr.flush()
        os.dup2(descriptor,1);os.dup2(descriptor,2)
    finally:os.close(descriptor)
    print(f'\nTerminal Workbench Preview launch: {datetime.now(timezone.utc).isoformat()}',flush=True)
    return path
