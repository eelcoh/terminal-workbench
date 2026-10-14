"""Explicit bounded shutdown of confirmed terminal identities; organization survives."""
from concurrent.futures import ThreadPoolExecutor
import os
import stat
import subprocess
import workspace_catalog as catalog


def check_endpoint(directory, terminal):
    path = catalog.socket_path(directory, terminal)
    for endpoint in (path, path.with_suffix(".ctl")):
        try:
            metadata = endpoint.lstat()
        except FileNotFoundError:
            continue
        if not stat.S_ISSOCK(metadata.st_mode) or metadata.st_uid != os.getuid():
            raise ValueError("Session endpoint is not a user-owned socket: " + terminal["id"])
    live = catalog.endpoint_alive(path.with_suffix(".ctl")) or catalog.endpoint_alive(path)
    if live and (not path.exists() or not path.with_suffix(".ctl").exists()):
        raise ValueError("Terminal cannot be stopped through this runtime: " + terminal["id"])
    return path, live


def finish(directory, runtime, data):
    catalog.validate(data)
    targets = data.get("pending_stop")
    if targets is None:
        return
    def stop(identity):
        try:
            path, live = check_endpoint(directory, {"id": identity})
            if live:
                result = subprocess.run([str(runtime), "stop", str(path)], capture_output=True, text=True, timeout=15)
                if result.returncode:
                    return identity + ": " + (result.stderr.strip() or "Session stop failed")
            return None
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            return identity + ": " + str(error)
    # This is explicit lifecycle work, never a render/PTY path. Bound parallelism
    # and per-process deadlines so a full catalog does not wait serially per shell.
    with ThreadPoolExecutor(max_workers=min(32, len(targets))) as workers:
        failures = [error for error in workers.map(stop, targets) if error]
    if failures:
        raise ValueError("Terminal shutdown pending; retry to finish. " + failures[0])
    del data["pending_stop"]
    catalog.save(directory, data)


def begin(directory, runtime, data, targets):
    known = {t["id"]: t for w in data["workspaces"] for t in w["terminals"]}
    if not isinstance(targets, list) or not targets or len(targets) > catalog.MAX_WORKSPACES * catalog.MAX_TERMINALS:
        raise ValueError("Choose terminals to stop")
    if any(not isinstance(i, str) or i not in known for i in targets) or len(set(targets)) != len(targets):
        raise ValueError("Terminal selection changed; reopen the confirmation")
    # Validate the entire selection before recording intent or stopping anything.
    for identity in targets:
        check_endpoint(directory, known[identity])
    data["pending_stop"] = targets.copy()
    catalog.save(directory, data)
    finish(directory, runtime, data)
