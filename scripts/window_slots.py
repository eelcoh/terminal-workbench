"""Client window slots, leased for the lifetime of the viewer process."""
from contextlib import contextmanager
from pathlib import Path
import fcntl
import os
import stat


def _directory(sessions):
    directory = Path(sessions) / "windows"
    directory.mkdir(mode=0o700, exist_ok=True)
    metadata = directory.lstat()
    if not stat.S_ISDIR(metadata.st_mode) or metadata.st_uid != os.getuid() or metadata.st_mode & 0o022:
        raise ValueError("Window state directory must be user-owned and private")
    return directory


def _acquire(directory,slot):
    descriptor = os.open(directory / f"{slot}.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid != os.getuid():
            raise ValueError("Unsafe window slot lock")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            os.close(descriptor)
            return None
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def acquire_window_slot(sessions):
    directory = _directory(sessions)
    for slot in range(1, 65):
        descriptor = _acquire(directory,slot)
        if descriptor is not None:
            return descriptor, directory / f"{slot}.view"
    raise ValueError("All 64 window slots are in use")


@contextmanager
def reserve_window_slots(sessions):
    """Exclude every supported launcher while explicitly changing preview builds.

    Hold the same leases old launchers understand, including unused slots. A live
    viewer refuses activation before any service request; no waiting or eviction.
    """
    directory=_directory(sessions)
    descriptors=[]
    try:
        for slot in range(1,65):
            descriptor=_acquire(directory,slot)
            if descriptor is None:
                raise ValueError('Close all preview windows before activating another build; shells may remain running')
            descriptors.append(descriptor)
        yield
    finally:
        for descriptor in descriptors:os.close(descriptor)
