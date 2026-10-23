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

def directory_value(value, base=None, *, available=False):
    if not isinstance(value,str) or not value or len(os.fsencode(value))>1023 or any(ord(ch)<32 or ord(ch)==127 for ch in value):
        raise ValueError("Enter a folder path of at most 1023 bytes without control characters")
    path=Path(value).expanduser()
    if base is not None:
        if not path.is_absolute(): path=Path(base)/path
        path=path.resolve()
    if not path.is_absolute(): raise ValueError("Saved workspace folder must be absolute")
    if len(os.fsencode(path))>1023: raise ValueError("Resolved folder path exceeds 1023 bytes")
    if available and (not path.is_dir() or not os.access(path,os.R_OK | os.X_OK)):
        raise ValueError("Folder does not exist or cannot be accessed")
    return str(path)

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
        # Persist the renamed catalog/intent, where directory fsync is supported.
        parent = os.open(path.parent, os.O_RDONLY)
        try:
            try:
                os.fsync(parent)
            except OSError as error:
                if error.errno not in (errno.EINVAL, errno.ENOTSUP):
                    raise
        finally:
            os.close(parent)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def seed(directory):
    defaults = [("Main", ["spike", "main-2"]), ("Scratch", ["scratch-1", "scratch-2"])]
    legacy = directory / "workspaces.tsv"
    if legacy.exists():
        lines = legacy.read_text().splitlines()
        if lines and lines[0] in {"workbench-catalog-v2", "workbench-catalog-v3", "workbench-catalog-v4"}:
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
        if isinstance(workspace,dict) and "directory" in workspace:
            directory_value(workspace["directory"])
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
            if "custom_name" in entity and not isinstance(entity["custom_name"], bool):
                raise ValueError("Invalid custom terminal name flag")
        if not isinstance(workspace.get("terminals"), list) or not 0 <= len(workspace["terminals"]) <= MAX_TERMINALS:
            raise ValueError("Invalid terminal count")
    validate_pending(data)
    return data

def validate_pending(data):
    terminals = {t["id"] for w in data["workspaces"] for t in w["terminals"]}
    workspaces = {w["id"]: w for w in data["workspaces"]}
    if sum(key in data for key in ("pending_create", "pending_close", "pending_stop")) > 1:
        raise ValueError("Conflicting pending catalog operations")
    def identities(value, known):
        if not isinstance(value, list) or any(not isinstance(i, str) or i not in known for i in value) or len(set(value)) != len(value):
            raise ValueError("Invalid pending catalog identities")
    if "pending_stop" in data:
        identities(data["pending_stop"], terminals)
        if not data["pending_stop"]:
            raise ValueError("Empty pending shutdown")
    if "pending_create" in data:
        identities(data["pending_create"], terminals)
        if not data["pending_create"]:
            raise ValueError("Empty pending creation")
    if "pending_close" in data:
        pending = data["pending_close"]
        if not isinstance(pending, dict) or set(pending) != {"terminals", "workspaces"}:
            raise ValueError("Invalid pending close metadata")
        identities(pending["terminals"], terminals)
        identities(pending["workspaces"], workspaces)
        if not pending["terminals"] and not pending["workspaces"]:
            raise ValueError("Empty pending close")
        if any(t["id"] not in pending["terminals"] for i in pending["workspaces"] for t in workspaces[i]["terminals"]):
            raise ValueError("Pending workspace close omits terminals")

def load(directory):
    path = directory / "workspaces.json"
    return validate(json.loads(path.read_text()) if path.exists() else seed(directory))

def socket_path(directory, terminal):
    path = directory / f"{terminal['id']}.sock"
    if len(os.fsencode(path)) >= 104 or any(ch in str(path) for ch in "\t\r\n"):
        raise ValueError(f"Session socket path exceeds 103 bytes or contains a tab/newline: {path}. Use a shorter checkout path")
    return path

def check_sockets(directory, data):
    """Every saved terminal's socket path must fit the platform limit."""
    for workspace in data["workspaces"]:
        for terminal in workspace["terminals"]:
            socket_path(directory, terminal)

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


def prepare_launch(socket):
    # Reset previous evidence even when an older runtime will be launched.
    path=socket.with_suffix('.lifecycle')
    try:
        metadata=path.lstat()
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid!=os.getuid():
            raise ValueError('Lifecycle path is not a user-owned regular file')
    except FileNotFoundError:pass
    atomic_write(path,json.dumps(dict(version=1,event='launching',runtime_pid=None,shell_pid=None,
        started_ms=time.time_ns()//1_000_000,exit_code=None,signal=None))+'\n')

def ensure_sessions(directory, data, runtime, cwd, *, on_spawn=None, keep_started=False):
    started = []
    ready = set()
    try:
        for workspace in data["workspaces"]:
            for terminal in workspace["terminals"]:
                path = socket_path(directory, terminal)
                if path.exists():
                    if not (endpoint_alive(path.with_suffix(".ctl")) or endpoint_alive(path)):
                        raise ValueError(f"Session {terminal['id']} has a socket but no listening server ({path}). Run just recover-sessions, then just spike")
                    continue
                prepare_launch(path)
                log_path = directory / f"{terminal['id']}.log"
                with log_path.open("ab") as log:
                    process = subprocess.Popen([str(runtime), str(path)], cwd=directory_value(workspace.get("directory",str(cwd)),cwd,available=True),
                        stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True,
                        env={key: value for key, value in os.environ.items() if not key.startswith("WORKBENCH_")}
                            | {"WORKBENCH_AGENT_BIN": str(Path(__file__).resolve().parent.parent / ".local/bin")})
                started.append(process)
                deadline = time.monotonic() + 5
                while not path.exists():
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise ValueError(f"Shell failed to start; inspect {log_path.name}")
                    time.sleep(0.02)
                ready.add(process.pid)
                if on_spawn is not None:
                    on_spawn(terminal["id"], process)
        return started
    except Exception:
        for process in started:
            if (not keep_started or process.pid not in ready) and process.poll() is None:
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
        terminal["custom_name"] = True
    else:
        raise ValueError("Unknown management action")
    validate(data)

def save(directory, data):
    validate(data)
    check_sockets(directory, data)
    atomic_write(directory / "workspaces.json", json.dumps(data, ensure_ascii=False, indent=2) + "\n")

def finish_create(directory, data):
    """Reconcile durable creation intent without starting or stopping a shell."""
    validate(data)
    if "pending_create" not in data:
        return False
    # A saved identity survives whether its launch succeeded, failed or never ran.
    # Live endpoints are adopted normally; absent ones require explicit recovery.
    del data["pending_create"]
    save(directory, data)
    return True

def finish_close(directory, runtime, data):
    validate(data)
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

def finish_stop(directory, runtime, data):
    if "pending_stop" in data:
        import session_shutdown
        session_shutdown.finish(directory, runtime, data)

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

def manage(directory, runtime, cwd, method="initialize", identity="", title="", destination="", position=-1, *, on_spawn=None, project_directory=None):
    directory = directory.resolve()
    with locked(directory):
        saved_catalog = (directory / "workspaces.json").exists()
        data = load(directory)
        finish_create(directory, data)
        finish_stop(directory, runtime, data)
        explicit_recovery = method == "session.recover"
        # Recover interrupted close before any startup, never resurrect its shells.
        if method == "session.recover":
            recover_sessions(directory, data)
            method = "initialize"
        finish_close(directory, runtime, data)
        if method in {"terminal.close", "workspace.close"}:
            close_sessions(directory, runtime, data, method, identity)
            return data
        old_ids = {terminal["id"] for workspace in data["workspaces"] for terminal in workspace["terminals"]}
        # Directory changes are organization facts; never send cd to live shells.
        if method in {"workspace.directory", "workspace.create_at"}:
            selected=directory_value(project_directory,cwd,available=True)
            if method=="workspace.directory":
                workspace=next((w for w in data["workspaces"] if w["id"]==identity),None)
                if workspace is None: raise ValueError("Workspace no longer exists")
                workspace["directory"]=selected
                save(directory,data)
                return data
            mutate(data,"workspace.create",identity,title)
            data["workspaces"][-1]["directory"]=selected
            method="initialize-new"
        if method not in {"initialize","initialize-new"}:
            mutate(data, method, identity, title, destination, position)
        check_sockets(directory, data)  # Before committing intent.
        startup = data if method == "initialize" else {"workspaces": [
            {**workspace, "terminals": [terminal for terminal in workspace["terminals"] if terminal["id"] not in old_ids]}
            for workspace in data["workspaces"]
        ]}
        launch = method == "initialize" or method.endswith(".create") or method == "initialize-new"
        if saved_catalog and method == "initialize" and not explicit_recovery:
            launch = False
        launch_ids = [t["id"] for w in startup["workspaces"] for t in w["terminals"]] if launch else []
        if launch_ids:
            # Commit identities BEFORE spawning; a crash leaves recoverable intent.
            # A partial launch must not kill successful independent shells.
            for workspace in startup["workspaces"]:
                if workspace["terminals"]:
                    directory_value(workspace.get("directory", str(cwd)), cwd, available=True)
            data["pending_create"] = launch_ids
            validate(data)
            atomic_write(directory / "workspaces.json", json.dumps(data, ensure_ascii=False, indent=2) + "\n")
            ensure_sessions(directory, startup, runtime, cwd, keep_started=True,
                            **({"on_spawn": on_spawn} if on_spawn else {}))
            del data["pending_create"]
        save(directory, data)
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
    parser.add_argument("--directory", default=None)
    parser.add_argument("--position", type=int, default=-1)
    parser.add_argument("--terminals", default="")
    args = parser.parse_args()
    try:
        from session_service import request, exchange, PROTOCOL, Unavailable
        if args.command in {"terminal.status","terminal.lifecycle"}:
            # Background presentation checks do not restart an explicitly stopped service.
            try:
                reply = exchange(args.state_dir.resolve(), dict(protocol=PROTOCOL, method=args.command, identity=args.id))
            except ValueError as error:
                if str(error) != "Unknown session service method":
                    raise
                # Live services keep their older Python code after git pull.
                if args.command=="terminal.lifecycle":
                    try:reply=exchange(args.state_dir.resolve(),dict(protocol=PROTOCOL,method="terminal.status",identity=args.id))
                    except ValueError as legacy_error:
                        if str(legacy_error)!="Unknown session service method":raise
                        reply=None
                else:reply=None
                health = exchange(args.state_dir.resolve(), dict(protocol=PROTOCOL, method="health")) if reply is None else {"terminals":[reply]}
                terminal = next((t for t in health["terminals"] if t["id"] == args.id), None)
                if terminal is None:
                    raise ValueError("Terminal no longer exists")
                reply = dict(id=terminal["id"], state=terminal["state"])
            if args.command=="terminal.lifecycle":
                message=reply.get("lifecycle",{}).get("message", "Runtime stopped; no exit reason was recorded." if reply["state"]=="stopped" else "")
                message=str(message).replace("\t"," ").replace("\r"," ").replace("\n"," ").encode("utf-8")[:180].decode("utf-8",errors="ignore")
                print("LIFECYCLE\t"+reply["id"]+"\t"+reply["state"]+"\t"+message,flush=True)
            else:print("STATE\t" + reply["id"] + "\t" + reply["state"], flush=True)
            return 0
        data = request(args.state_dir, args.runtime, args.cwd, args.command, args.id, args.name, args.to, args.position, project_directory=args.directory, terminal_ids=args.terminals.split(",") if args.terminals else None)["data"]
        # Return the identity from this operation's locked result, never infer
        # it from a later catalog that another window may have changed.
        if args.command in {"workspace.create", "workspace.create_at", "terminal.create"}:
            workspace = data["workspaces"][-1] if args.command in {"workspace.create","workspace.create_at"} else next(
                item for item in data["workspaces"] if item["id"] == args.id)
            print("CREATED\t" + workspace["terminals"][-1]["id"] + "\t" + workspace["id"], flush=True)
        print("OK", flush=True)
        return 0
    except Unavailable:
        print("ERROR\tSession service unavailable; relaunch Workbench, then Check again",flush=True)
        return 1
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print("ERROR\t" + str(error).replace("\n", " ").replace("\t", " ")[:400], flush=True)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
