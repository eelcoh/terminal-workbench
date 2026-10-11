"""Client window slots, leased for the lifetime of the viewer process."""
from pathlib import Path
import fcntl
import os
import stat


def acquire_window_slot(sessions):
    directory = Path(sessions) / "windows"
    directory.mkdir(mode=0o700, exist_ok=True)
    metadata = directory.lstat()
    if not stat.S_ISDIR(metadata.st_mode) or metadata.st_uid != os.getuid() or metadata.st_mode & 0o022:
        raise ValueError("Window state directory must be user-owned and private")
    for slot in range(1, 65):
        descriptor = os.open(directory / f"{slot}.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid != os.getuid():
                raise ValueError("Unsafe window slot lock")
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                os.close(descriptor)
                continue
            return descriptor, directory / f"{slot}.view"
        except BaseException:
            os.close(descriptor)
            raise
    raise ValueError("All 64 window slots are in use")
