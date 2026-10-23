#!/usr/bin/env python3
"""Client for the Rust session service (`band-session service`).

Development tools and tests use it to start the service and send protocol 1
requests. Clients retry only connection establishment, never a sent mutation.
"""
from pathlib import Path
import argparse
import errno
import fcntl
import json
import os
import signal
import socket
import subprocess
import sys
import time

PROTOCOL = 1
LIMIT = 256 * 1024

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


def stop_owned_startup(process):
    """Reap only a process group spawned by this caller with a new session."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=1)
    except subprocess.TimeoutExpired:
        pass
    finally:
        # The leader may exit before a descendant that ignores SIGTERM.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()


def _wait_for_startup(directory, message, child, *, timeout=5):
    deadline = time.monotonic() + timeout
    try:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise ValueError(f"Session service did not start; inspect {directory / 'service.log'}")
            try:
                # Only readiness is retried. A mutation is sent after this
                # function returns, outside failed-start cleanup ownership.
                exchange(directory, dict(message, method="health"), timeout=remaining)
                return
            except (Unavailable, socket.timeout):
                if child.poll() is not None or time.monotonic() >= deadline:
                    raise ValueError(f"Session service did not start; inspect {directory / 'service.log'}")
                time.sleep(min(.02, max(0, deadline - time.monotonic())))
    except BaseException:
        # Once startup fails or is interrupted, this child must not become an
        # unexpected authority later. Retained Rust shell runtimes are separate.
        stop_owned_startup(child)
        raise


def service_command(directory, runtime, cwd):
    """Start the Rust session authority."""
    agent_bin = Path(__file__).resolve().parent.parent / ".local/bin"
    return [str(runtime), "service", "--state-dir", str(directory), "--runtime", str(runtime),
            "--cwd", str(cwd), "--agent-bin", str(agent_bin)]


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
            child = subprocess.Popen(service_command(directory, runtime, cwd),
                stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True,
                close_fds=True)
        _wait_for_startup(directory, message, child)
        return exchange(directory, message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("status", "stop", "recover", "recover-all"))
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, required=True)
    parser.add_argument("--id", default="")
    args = parser.parse_args()
    try:
        method = {"status": "health", "stop": "service.shutdown", "recover": "terminal.recover", "recover-all": "session.recover"}[args.action]
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
