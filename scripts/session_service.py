#!/usr/bin/env python3
"""Spike session authority: Unix JSON requests, catalog lifecycle and health.

PTY streams remain with independent Rust runtimes. Stopping this service never
stops them. Clients retry only connection establishment, never a sent mutation.
"""
from pathlib import Path
import argparse
import errno
import fcntl
import json
import os
import signal
import socket
import stat
import subprocess
import sys
import time
import uuid

PROTOCOL = 1
LIMIT = 256 * 1024
METHODS = {"catalog.read", "initialize", "workspace.create", "workspace.rename", "workspace.close",
           "terminal.create", "terminal.rename", "terminal.close", "terminal.move",
           "workspace.create_at", "workspace.directory", "session.recover", "terminal.recover", "terminal.status", "terminal.lifecycle", "health", "service.shutdown", "service.retire", "session.stop"}

class Unavailable(Exception):
    pass


class ReplyClosed(ValueError):
    """A sent request lost its reply; mutations must never be replayed."""
    pass


def endpoint(directory):
    path = directory / "service.rpc"
    if len(os.fsencode(path)) >= 104:
        raise ValueError("Session service socket exceeds 103 bytes; use a shorter checkout path")
    return path


def receive(connection, *, deadline=None):
    data = bytearray()
    while b"\n" not in data:
        if deadline is not None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise socket.timeout('Session service reply deadline expired')
            connection.settimeout(remaining)
        chunk = connection.recv(min(4096, LIMIT + 1 - len(data)))
        if not chunk:
            raise ReplyClosed("Session service closed before replying")
        data.extend(chunk)
        if len(data) > LIMIT:
            raise ValueError("Session service message exceeds size limit")
    line, remainder = data.split(b"\n", 1)
    if remainder:
        raise ValueError("Only one request per connection is supported")
    value = json.loads(line)
    if not isinstance(value, dict):
        raise ValueError("Session service message must be an object")
    return value


def transmit(connection, value):
    payload = json.dumps(value, ensure_ascii=True, separators=(",", ":")).encode() + b"\n"
    if len(payload) > LIMIT:
        raise ValueError("Session service reply exceeds size limit")
    connection.sendall(payload)


def exchange(directory, message, *, timeout=None):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(60 if timeout is None else timeout)
        try:
            connection.connect(str(endpoint(directory)))
        except OSError as error:
            if error.errno in (errno.ENOENT, errno.ECONNREFUSED):
                raise Unavailable() from error
            raise
        deadline = None if timeout is None else time.monotonic() + timeout
        transmit(connection, message)
        reply = receive(connection, deadline=deadline)
    if reply.get("protocol") != PROTOCOL:
        raise ValueError("Incompatible session service; stop it explicitly before restarting")
    if not reply.get("ok"):
        raise ValueError(reply.get("error", "Session service rejected the request"))
    return reply


def prepare_directory(directory):
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    metadata = directory.stat()
    if metadata.st_uid != os.getuid() or metadata.st_mode & 0o022:
        raise ValueError("Session directory must be owned by you and not writable by other users")


def request(directory, runtime, cwd, method="initialize", identity="", title="", destination="", position=-1, *, project_directory=None, terminal_ids=None, expected_instance=None):
    directory, runtime, cwd = directory.resolve(), runtime.resolve(), cwd.resolve()
    message = dict(protocol=PROTOCOL, method=method, identity=identity, title=title,
                   destination=destination, position=position, runtime=str(runtime), cwd=str(cwd), project_directory=project_directory)
    if method == "service.retire":
        message["expected_instance"] = expected_instance
    if method == "session.stop":
        message["terminal_ids"] = terminal_ids
    try:
        return exchange(directory, message)
    except Unavailable:
        if method in {"catalog.read", "service.retire"}:
            raise
        if method == "service.shutdown":
            return dict(protocol=PROTOCOL, ok=True, stopped=True)
    prepare_directory(directory)
    endpoint(directory)
    # Serialize concurrent first launches. A separate lock is held by the server
    # for its lifetime; competing direct server starts cannot replace its socket.
    with (directory / "service-start.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        try:
            return exchange(directory, message)
        except Unavailable:
            pass
        with (directory / "service.log").open("ab", buffering=0) as log:
            child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "serve",
                "--state-dir", str(directory), "--runtime", str(runtime), "--cwd", str(cwd)],
                stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True,
                close_fds=True)
        deadline = time.monotonic() + 5
        while True:
            try:
                # Probe readiness separately: a failed mutation is not replayed.
                exchange(directory, dict(message, method="health"))
                break
            except Unavailable:
                if child.poll() is not None or time.monotonic() >= deadline:
                    raise ValueError(f"Session service did not start; inspect {directory / 'service.log'}")
                time.sleep(.02)
        return exchange(directory, message)


class Authority:
    def __init__(self, directory, runtime, cwd):
        self.directory, self.runtime, self.cwd = directory, runtime, cwd
        self.instance = uuid.uuid4().hex
        self.children = {}
        self.capabilities = {}
        self.running = True

    def started(self, identity, child):
        self.children[identity] = child

    def health(self):
        import workspace_catalog as catalog
        # Reading health never seeds the catalog or starts a terminal.
        with catalog.locked(self.directory):
            data = catalog.load(self.directory) if (self.directory / "workspaces.json").exists() else {"workspaces": []}
            terminals = []
            for workspace in data["workspaces"]:
                for terminal in workspace["terminals"]:
                    path = catalog.socket_path(self.directory, terminal)
                    child = self.children.get(terminal["id"])
                    if child is not None:
                        child.poll()  # Reap exited children; do not restart them.
                    try:
                        alive = catalog.endpoint_alive(path.with_suffix(".ctl")) or catalog.endpoint_alive(path)
                        state = "running" if alive else "stopped"
                        issue = "" if alive else "Runtime stopped; explicit recovery starts a new shell"
                    except (OSError, ValueError) as error:
                        state, issue = "unavailable", str(error)
                    compatibility = self.runtime_capabilities(path) if state == "running" else None
                    from terminal_lifecycle import detail
                    lifecycle=detail(path,state)
                    if state=="stopped":issue=lifecycle["message"]
                    terminals.append(dict(lifecycle=lifecycle,id=terminal["id"], workspace_id=workspace["id"],
                        workspace_name=workspace["name"], name=terminal["name"], state=state,
                        runtime=compatibility, issue=issue))
        current_controls = {str(catalog.socket_path(self.directory, t).with_suffix(".ctl"))
                            for w in data["workspaces"] for t in w["terminals"]}
        self.capabilities = {key: value for key, value in self.capabilities.items() if key in current_controls}
        return dict(service_pid=os.getpid(), service_instance=self.instance, terminals=terminals,
                    runtime_binary=str(self.runtime), directory=str(self.cwd), methods=sorted(METHODS),
                    features=["catalog-journal-v1"])

    def runtime_capabilities(self, path):
        control = path.with_suffix(".ctl")
        try:
            metadata = control.stat()
        except FileNotFoundError:
            return dict(capabilities=[], compatibility="legacy-replay")
        key = (metadata.st_dev, metadata.st_ino, metadata.st_mtime_ns)
        cached = self.capabilities.get(str(control))
        if cached and cached[0] == key:
            return cached[1]
        result = dict(capabilities=None, compatibility="unadvertised")
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                connection.settimeout(1)
                connection.connect(str(control))
                connection.sendall(b"CAPS\n")
                advertised = receive(connection)
                if advertised.get("protocol") == 1 and isinstance(advertised.get("capabilities"), list):
                    result = advertised | {"compatibility": "advertised"}
        except (OSError, ValueError):
            pass
        self.capabilities[str(control)] = (key, result)
        return result

    def dispatch(self, message):
        import workspace_catalog as catalog
        if message.get("protocol") != PROTOCOL:
            raise ValueError("Incompatible session service protocol")
        method = message.get("method")
        if method not in METHODS:
            raise ValueError("Unknown session service method")
        identity, title, destination = (message.get(key, "") for key in ("identity", "title", "destination"))
        position = message.get("position", -1)
        project_directory=message.get("project_directory")
        if project_directory is not None and (not isinstance(project_directory,str) or len(os.fsencode(project_directory))>1023):
            raise ValueError("Invalid workspace folder")
        if not all(isinstance(value, str) and len(value) <= 1024 for value in (identity, title, destination)) or type(position) is not int:
            raise ValueError("Invalid session service parameters")
        if method == "service.retire":
            # Runtime authority, independent of any client's window model. A
            # stale observer must never retire a replacement service instance.
            if message.get("expected_instance") != self.instance:
                raise ValueError("Session service changed; inspect its status before retrying")
            self.running = False
            return dict(retired=True)
        if method == "service.shutdown":
            self.running = False
            return dict(stopped=True)
        if method == "health":
            return self.health()
        if method == "catalog.read":
            # Read committed metadata only: no journal reconciliation, seeding,
            # terminal probing, recovery, ownership changes or catalog rewrite.
            with catalog.locked(self.directory):
                if not (self.directory / "workspaces.json").exists():
                    raise ValueError("Saved workspace catalog is unavailable")
                data = catalog.load(self.directory)
            return dict(data=data)

        if method in {"terminal.status","terminal.lifecycle"}:
            # Read one runtime fact without attachment or capability enumeration.
            with catalog.locked(self.directory):
                data = catalog.load(self.directory)
                terminal = next((t for w in data["workspaces"] for t in w["terminals"] if t["id"] == identity), None)
                if terminal is None:
                    raise ValueError("Terminal no longer exists")
                path = catalog.socket_path(self.directory, terminal)
                try:
                    for candidate in (path.with_suffix(".ctl"), path):
                        try:
                            metadata = candidate.lstat()
                        except FileNotFoundError:
                            continue
                        if not stat.S_ISSOCK(metadata.st_mode) or metadata.st_uid != os.getuid():
                            raise ValueError("Session endpoint is not a user-owned socket")
                    alive = catalog.endpoint_alive(path.with_suffix(".ctl")) or catalog.endpoint_alive(path)
                    state = "running" if alive else "stopped"
                except (OSError, ValueError):
                    state = "unavailable"
            if method=="terminal.lifecycle":
                from terminal_lifecycle import detail
                return dict(id=identity,state=state,lifecycle=detail(path,state))
            return dict(id=identity, state=state)
        if message.get("runtime") != str(self.runtime) or message.get("cwd") != str(self.cwd):
            raise ValueError("Session service configuration differs; stop the service before changing runtime or directory")
        if method == "initialize":
            # Existing identities without a live runtime are retained as stopped.
            # Only initial seeding or explicit recovery can restart saved shells.
            with catalog.locked(self.directory):
                if (self.directory / "workspaces.json").exists() or (self.directory / "workspaces.tsv").exists():
                    data = catalog.load(self.directory)
                    catalog.finish_create(self.directory, data)
                    catalog.finish_stop(self.directory, self.runtime, data)
                    catalog.finish_close(self.directory, self.runtime, data)
                    catalog.save(self.directory, data)
                    return dict(data=data, service_pid=os.getpid())
        if method == "session.stop":
            import session_shutdown
            with catalog.locked(self.directory):
                data = catalog.load(self.directory)
                catalog.finish_create(self.directory, data)
                catalog.finish_stop(self.directory, self.runtime, data)
                catalog.finish_close(self.directory, self.runtime, data)
                session_shutdown.begin(self.directory, self.runtime, data, message.get("terminal_ids"))
            return dict(data=data, service_pid=os.getpid())
        if method == "terminal.recover":
            with catalog.locked(self.directory):
                data = catalog.load(self.directory)
                catalog.finish_create(self.directory, data)
                catalog.finish_stop(self.directory, self.runtime, data)
                catalog.finish_close(self.directory, self.runtime, data)
                terminal = next((t for w in data["workspaces"] for t in w["terminals"] if t["id"] == identity), None)
                if terminal is None:
                    raise ValueError("Terminal no longer exists")
                path = catalog.socket_path(self.directory, terminal)
                if catalog.endpoint_alive(path.with_suffix(".ctl")) or catalog.endpoint_alive(path):
                    raise ValueError("Terminal is still running; recovery would replace a live shell")
                workspace=next(w for w in data["workspaces"] if terminal in w["terminals"])
                subset = {"workspaces": [{**workspace, "terminals": [terminal]}]}
                catalog.directory_value(workspace.get("directory",str(self.cwd)),self.cwd,available=True)
                catalog.recover_sessions(self.directory, subset)
                catalog.ensure_sessions(self.directory, subset, self.runtime, self.cwd, on_spawn=self.started)
            return dict(data=data, service_pid=os.getpid())
        data = catalog.manage(self.directory, self.runtime, self.cwd, method, identity, title,
                              destination, position, on_spawn=self.started, project_directory=project_directory)
        return dict(data=data, service_pid=os.getpid())


def serve(directory, runtime, cwd):
    prepare_directory(directory)
    path = endpoint(directory)
    with (directory / "service.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if path.exists() or path.is_symlink():
            metadata = path.lstat()
            if not stat.S_ISSOCK(metadata.st_mode) or metadata.st_uid != os.getuid():
                raise ValueError("Refusing to replace a non-socket or unowned service endpoint")
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe:
                try:
                    probe.connect(str(path))
                except OSError as error:
                    if error.errno not in (errno.ENOENT, errno.ECONNREFUSED):
                        raise
                else:
                    raise ValueError("Service endpoint is already listening")
            path.unlink()
        authority = Authority(directory, runtime, cwd)
        def stop(signum, frame):
            authority.running = False
        signal.signal(signal.SIGTERM, stop)
        signal.signal(signal.SIGINT, stop)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
            listener.bind(str(path))
            os.chmod(path, 0o600)
            socket_inode = path.stat().st_ino
            listener.listen(32)
            listener.settimeout(1)
            print(f"Session service {os.getpid()} listening at {path}", flush=True)
            retired = False
            def retire_endpoint():
                nonlocal retired
                if retired:
                    return
                # Shutdown acknowledgement is a barrier: nobody may connect to a
                # listener that will never accept again. Retire before replying,
                # then release ownership so an immediate reopen can start a service.
                listener.close()
                try:
                    if path.lstat().st_ino == socket_inode:
                        path.unlink()
                except FileNotFoundError:
                    pass
                retired = True
                fcntl.flock(lock, fcntl.LOCK_UN)
            try:
                while authority.running and directory.exists():
                    for identity, child in list(authority.children.items()):
                        if child.poll() is not None:
                            del authority.children[identity]
                    try:
                        connection, _ = listener.accept()
                    except socket.timeout:
                        continue
                    with connection:
                        connection.settimeout(2)
                        try:
                            message = receive(connection)
                            result = authority.dispatch(message)
                            if not authority.running:
                                retire_endpoint()
                            transmit(connection, dict(protocol=PROTOCOL, ok=True, **result))
                        except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
                            try:
                                transmit(connection, dict(protocol=PROTOCOL, ok=False, error=str(error)[:1000]))
                            except OSError:
                                pass
            finally:
                # Only our endpoint is removed. Runtimes and their sockets survive.
                retire_endpoint()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("serve", "status", "stop", "recover"))
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, required=True)
    parser.add_argument("--id", default="")
    args = parser.parse_args()
    try:
        if args.action == "serve":
            serve(args.state_dir.resolve(), args.runtime.resolve(), args.cwd.resolve())
            return 0
        method = {"status": "health", "stop": "service.shutdown", "recover": "terminal.recover"}[args.action]
        result = request(args.state_dir, args.runtime, args.cwd, method, args.id)
        if args.action == "status":
            print(f"Session service: {result['service_pid']}")
            for terminal in result["terminals"]:
                compatibility = terminal["runtime"]["compatibility"] if terminal["runtime"] else "no runtime"
                print(f"{terminal['id']}  {terminal['workspace_name']} / {terminal['name']}: {terminal['state']} ({compatibility})")
                if terminal["issue"]:
                    print(f"  {terminal['issue']}")
        else:
            print("OK")
        return 0
    except (ValueError, OSError, Unavailable, subprocess.SubprocessError) as error:
        print(f"Session service: {error}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
