#!/usr/bin/env python3
"""Persistent session organization and a local management CLI for the spike."""
from contextlib import contextmanager
from pathlib import Path
import argparse
import errno
import fcntl
import json
import os
import subprocess
import socket
import stat
import tempfile
import time
import uuid

MAX_WORKSPACES = 8
MAX_TERMINALS = 8

def name(value):
    if not isinstance(value, str) or not value.strip():
        raise ValueError("Enter a name")
    if len(value.encode()) > 63 or any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise ValueError("Names must fit 63 UTF-8 bytes and contain no control characters")
    return value.strip()

@contextmanager
def locked(directory):
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    with (directory / "catalog.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield

def atomic_write(path, value):
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w") as file:
            file.write(value)
            file.flush()
            os.fsync(file.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def seed(directory):
    defaults = [("Main", ["spike", "main-2"]), ("Scratch", ["scratch-1", "scratch-2"])]
    legacy = directory / "workspaces.tsv"
    if legacy.exists():
        lines = legacy.read_text().splitlines()
        if lines and lines[0] in {"workbench-catalog-v2", "workbench-catalog-v3"}:
            raise ValueError("Saved catalog found without JSON metadata; existing files were preserved")
        if lines:
            defaults = []
            for row in lines:
                fields = row.split("\t")
                if len(fields) != 3:
                    raise ValueError("Invalid old catalog; its contents were preserved")
                if any(Path(path).resolve().parent != directory.resolve() or Path(path).suffix != ".sock" for path in fields[1:]):
                    raise ValueError("Old catalog paths must belong to this session directory")
                defaults.append((name(fields[0]), [Path(path).stem for path in fields[1:]]))
    return {"version": 1, "workspaces": [
        {"id": f"workspace-{w}", "name": title, "terminals": [
            {"id": identity, "name": f"Shell {p + 1}"} for p, identity in enumerate(identities)
        ]} for w, (title, identities) in enumerate(defaults)
    ]}

def validate(data):
    if not isinstance(data, dict) or data.get("version") != 1:
        raise ValueError("Unsupported workspace metadata version")
    workspaces = data.get("workspaces")
    if not isinstance(workspaces, list) or not 0 <= len(workspaces) <= MAX_WORKSPACES:
        raise ValueError("Invalid workspace count")
    ids = set()
    for workspace in workspaces:
        if not isinstance(workspace, dict) or not isinstance(workspace.get("terminals"), list) or not 0 <= len(workspace["terminals"]) <= MAX_TERMINALS:
            raise ValueError("Invalid terminal count")
        for entity in [workspace, *workspace["terminals"]]:
            if not isinstance(entity, dict):
                raise ValueError("Invalid session metadata")
            identity = entity.get("id", "")
            if not isinstance(identity, str) or not identity or len(identity) > 40 or not all(ch.isascii() and (ch.isalnum() or ch in "-_") for ch in identity) or identity in ids:
                raise ValueError("Invalid or duplicate session identity")
            ids.add(identity)
            if name(entity.get("name")) != entity["name"]:
                raise ValueError("Invalid saved name")
        if not isinstance(workspace.get("terminals"), list) or not 0 <= len(workspace["terminals"]) <= MAX_TERMINALS:
            raise ValueError("Invalid terminal count")
    return data

def load(directory):
    path = directory / "workspaces.json"
    return validate(json.loads(path.read_text()) if path.exists() else seed(directory))

def socket_path(directory, terminal):
    path = directory / f"{terminal['id']}.sock"
    if len(os.fsencode(path)) >= 104 or any(ch in str(path) for ch in "\t\r\n"):
        raise ValueError(f"Session socket path exceeds 103 bytes or contains a tab/newline: {path}. Use a shorter checkout path")
    return path

def snapshot(directory, data):
    lines = ["workbench-catalog-v3"]
    for workspace in data["workspaces"]:
        lines.append("\t".join(["W", workspace["id"], workspace["name"]]))
        for terminal in workspace["terminals"]:
            lines.append("\t".join(["T", workspace["id"], terminal["id"], terminal["name"], str(socket_path(directory, terminal))]))
    return "\n".join(lines) + "\n"

def endpoint_alive(path):
    """Probe without sending input. Only absence/refusal establishes a dead endpoint."""
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(1)
        try:
            connection.connect(str(path))
            return True
        except OSError as error:
            if error.errno in (errno.ENOENT, errno.ECONNREFUSED):
                return False
            raise ValueError(f"Could not check session endpoint {path}: {error}") from error


def recover_sessions(directory, data):
    """Explicit recovery, under the catalog lock; never remove a listening endpoint."""
    stale = []
    for workspace in data["workspaces"]:
        for terminal in workspace["terminals"]:
            path = socket_path(directory, terminal)
            paths = [path.with_suffix(".ctl"), path]
            if any(endpoint_alive(endpoint) for endpoint in paths):
                continue
            for endpoint in paths:
                try:
                    metadata = endpoint.lstat()
                except FileNotFoundError:
                    continue
                if not stat.S_ISSOCK(metadata.st_mode) or metadata.st_uid != os.getuid():
                    raise ValueError(f"Refusing to recover a non-socket or unowned endpoint: {endpoint}")
                stale.append((endpoint, metadata.st_dev, metadata.st_ino))
    # Validate the whole plan before removing anything, then recheck each endpoint.
    for path, device, inode in stale:
        metadata = path.lstat()
        if (metadata.st_dev, metadata.st_ino) != (device, inode) or endpoint_alive(path):
            raise ValueError(f"Session endpoint changed during recovery: {path}; retry")
    for path, device, inode in stale:
        metadata = path.lstat()
        if (metadata.st_dev, metadata.st_ino) != (device, inode) or endpoint_alive(path):
            raise ValueError(f"Session endpoint changed during recovery: {path}; retry")
        path.unlink()


def ensure_sessions(directory, data, runtime, cwd):
    started = []
    try:
        for workspace in data["workspaces"]:
            for terminal in workspace["terminals"]:
                path = socket_path(directory, terminal)
                if path.exists():
                    if not (endpoint_alive(path.with_suffix(".ctl")) or endpoint_alive(path)):
                        raise ValueError(f"Session {terminal['id']} has a socket but no listening server ({path}). Run just recover-sessions, then just spike")
                    continue
                log_path = directory / f"{terminal['id']}.log"
                with log_path.open("ab") as log:
                    process = subprocess.Popen([str(runtime), str(path)], cwd=cwd,
                        stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True,
                        env={key: value for key, value in os.environ.items() if not key.startswith("WORKBENCH_")}
                            | {"WORKBENCH_AGENT_BIN": str(Path(__file__).resolve().parent.parent / ".local/bin")})
                started.append(process)
                deadline = time.monotonic() + 5
                while not path.exists():
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise ValueError(f"Shell failed to start; inspect {log_path.name}")
                    time.sleep(0.02)
        return started
    except Exception:
        for process in started:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
        raise

def move_terminal(data, identity, destination, position=-1):
    workspaces = data['workspaces']
    source = next((w for w in workspaces if any(t['id'] == identity for t in w['terminals'])), None)
    target = next((w for w in workspaces if w['id'] == destination), None)
    if source is None:
        raise ValueError('Terminal no longer exists')
    if target is None:
        raise ValueError('Destination workspace no longer exists')
    if source is not target and len(target['terminals']) >= MAX_TERMINALS:
        raise ValueError('Destination already has eight terminals')
    remaining = len(target['terminals']) - (source is target)
    if type(position) is not int:
        raise ValueError('Invalid terminal insertion position')
    if position == -1:
        position = remaining
    if not 0 <= position <= remaining:
        raise ValueError('Invalid terminal insertion position')
    terminal = next(t for t in source['terminals'] if t['id'] == identity)
    source['terminals'].remove(terminal)
    target['terminals'].insert(position, terminal)
    validate(data)


def new_terminal_identity(data):
    """Keep new socket names short without changing persisted terminal IDs."""
    used = {entity["id"] for workspace in data["workspaces"]
            for entity in [workspace, *workspace["terminals"]]}
    for _ in range(32):
        identity = uuid.uuid4().hex[:16]
        if identity not in used:
            return identity
    raise ValueError("Could not allocate a unique terminal identity; retry")


def mutate(data, method, identity, title, destination='', position=-1):
    if method == 'terminal.move':
        move_terminal(data, identity, destination, position)
        return
    title = name(title)
    workspaces = data["workspaces"]
    if method == "workspace.create":
        if len(workspaces) >= MAX_WORKSPACES:
            raise ValueError("This spike supports eight workspaces")
        workspaces.append({"id": uuid.uuid4().hex, "name": title,
            "terminals": [{"id": new_terminal_identity(data), "name": "Shell 1"}]})
    elif method in {"workspace.rename", "terminal.create"}:
        workspace = next((item for item in workspaces if item["id"] == identity), None)
        if workspace is None:
            raise ValueError("Workspace no longer exists")
        if method == "workspace.rename":
            workspace["name"] = title
        else:
            if len(workspace["terminals"]) >= MAX_TERMINALS:
                raise ValueError("This spike supports eight terminals per workspace")
            workspace["terminals"].append({"id": new_terminal_identity(data), "name": title})
    elif method == "terminal.rename":
        terminal = next((item for workspace in workspaces for item in workspace["terminals"] if item["id"] == identity), None)
        if terminal is None:
            raise ValueError("Terminal no longer exists")
        terminal["name"] = title
    else:
        raise ValueError("Unknown management action")
    validate(data)

def save(directory, data):
    validate(data)
    atomic_write(directory / "workspaces.json", json.dumps(data, ensure_ascii=False, indent=2) + "\n")
    atomic_write(directory / "workspaces.tsv", snapshot(directory, data))

def finish_close(directory, runtime, data):
    pending = data.get("pending_close")
    if not pending:
        return
    if not isinstance(pending, dict) or not isinstance(pending.get("terminals"), list) or not isinstance(pending.get("workspaces"), list):
        raise ValueError("Invalid pending close metadata")
    ids = pending["terminals"]
    existing = {terminal["id"] for workspace in data["workspaces"] for terminal in workspace["terminals"]}
    if any(identity not in existing for identity in ids) or any(identity not in {w["id"] for w in data["workspaces"]} for identity in pending["workspaces"]):
        raise ValueError("Unknown pending close identity")
    for identity in ids:
        path = socket_path(directory, {"id": identity})
        result = subprocess.run([str(runtime), "stop", str(path)], capture_output=True, text=True, timeout=15)
        if result.returncode:
            raise ValueError("Close pending; reopen to retry. " + (result.stderr.strip() or "Session stop failed"))
    for workspace in data["workspaces"]:
        workspace["terminals"] = [terminal for terminal in workspace["terminals"] if terminal["id"] not in ids]
    data["workspaces"] = [workspace for workspace in data["workspaces"] if workspace["id"] not in pending["workspaces"]]
    del data["pending_close"]
    save(directory, data)

def close_sessions(directory, runtime, data, method, identity):
    workspaces = data["workspaces"]
    affected = [terminal for workspace in workspaces for terminal in workspace["terminals"]
        if (workspace["id"] == identity if method == "workspace.close" else terminal["id"] == identity)]
    if method == "workspace.close":
        if not any(workspace["id"] == identity for workspace in workspaces):
            raise ValueError("Workspace no longer exists")
    elif not affected:
        raise ValueError("Terminal no longer exists")
    # Check every endpoint before recording destructive intent or stopping any shell.
    for terminal in affected:
        path = socket_path(directory, terminal)
        if path.exists() and not path.with_suffix(".ctl").exists():
            raise ValueError("Earlier spike session: exit its shell and reopen before using Stop")
    data["pending_close"] = {"terminals": [t["id"] for t in affected],
        "workspaces": [identity] if method == "workspace.close" else []}
    save(directory, data)
    finish_close(directory, runtime, data)

def manage(directory, runtime, cwd, method="initialize", identity="", title="", destination="", position=-1):
    directory = directory.resolve()
    with locked(directory):
        data = load(directory)
        # Recover interrupted close before any startup, never resurrect its shells.
        if method == "session.recover":
            recover_sessions(directory, data)
            method = "initialize"
        finish_close(directory, runtime, data)
        if method in {"terminal.close", "workspace.close"}:
            close_sessions(directory, runtime, data, method, identity)
            return data
        old_ids = {terminal["id"] for workspace in data["workspaces"] for terminal in workspace["terminals"]}
        if method != "initialize":
            mutate(data, method, identity, title, destination, position)
        rendered = snapshot(directory, data)
        startup = data if method == "initialize" else {"workspaces": [
            {"terminals": [terminal for terminal in workspace["terminals"] if terminal["id"] not in old_ids]}
            for workspace in data["workspaces"]
        ]}
        started = ensure_sessions(directory, startup, runtime, cwd) if method == "initialize" or method.endswith(".create") else []
        committed = False
        try:
            atomic_write(directory / "workspaces.json", json.dumps(data, ensure_ascii=False, indent=2) + "\n")
            committed = True
            atomic_write(directory / "workspaces.tsv", rendered)
        finally:
            # Before metadata commit these are unused new sessions; afterward
            # retain them so a later initialize can recover a failed projection.
            if not committed:
                for process in started:
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=5)
        return data

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, required=True)
    parser.add_argument("--command", default="initialize")
    parser.add_argument("--id", default="")
    parser.add_argument("--name", default="")
    parser.add_argument("--to", default="")
    parser.add_argument("--position", type=int, default=-1)
    args = parser.parse_args()
    try:
        manage(args.state_dir, args.runtime.resolve(), args.cwd.resolve(), args.command, args.id, args.name, args.to, args.position)
        print("OK", flush=True)
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print("ERROR\t" + str(error).replace("\n", " ").replace("\t", " ")[:400], flush=True)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
