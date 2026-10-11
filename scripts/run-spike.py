#!/usr/bin/env python3
"""Bootstrap persistent workspace sessions and attach the experimental viewer."""
from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess
import sys
import time
from session_service import request, exchange, Unavailable, PROTOCOL
from window_slots import acquire_window_slot

ROOT = Path(__file__).resolve().parent.parent


def inside_workbench(environment):
    return bool(environment.get("WORKBENCH_SESSION_SOCKET")) or environment.get("TERM_PROGRAM") == "terminal-workbench"


def launch_viewer(viewer, root, sessions, environment, *, detach=False):
    """Log viewer diagnostics; nested launches release their invoking terminal."""
    log_path = sessions / "viewer.log"
    slot_fd, view_path = acquire_window_slot(sessions)
    environment = dict(environment, WORKBENCH_WINDOW_STATE=str(view_path), WORKBENCH_WINDOW_SLOT_FD=str(slot_fd))
    print(f"Window view: {view_path.name}", flush=True)
    print(f"Viewer log: {log_path}", flush=True)
    try:
        return _launch_viewer(viewer, root, environment, detach, log_path, slot_fd)
    finally:
        os.close(slot_fd)


def _launch_viewer(viewer, root, environment, detach, log_path, slot_fd):
    with log_path.open("ab", buffering=0) as log:
        log.write(f"\n--- Viewer launch {datetime.now(timezone.utc).isoformat()} ---\n{viewer}\n".encode())
        try:
            process = subprocess.Popen([str(viewer)], cwd=root, env=environment,
                                       stdin=subprocess.DEVNULL if detach else None,
                                       stdout=log if detach else subprocess.PIPE,
                                       stderr=subprocess.STDOUT, start_new_session=detach,
                                       close_fds=True, pass_fds=(slot_fd,))
        except OSError as error:
            message = f"Could not launch viewer: {error}\n"
            log.write(message.encode())
            print(message, file=sys.stderr, end="")
            return 1
        log.write(f"Viewer pid: {process.pid}; independent: {detach}\n".encode())
        if detach:
            # A launcher started in a retained terminal must not become its
            # foreground job for the lifetime of the new window, or stream GUI
            # diagnostics back into that terminal's PTY.
            try:
                result = process.wait(timeout=0.25)
            except subprocess.TimeoutExpired:
                print(f"Viewer started independently (pid {process.pid}). Diagnostics: {log_path}", flush=True)
                return 0
            log.write(f"Viewer exit code: {result}\n".encode())
            if result:
                print(f"Viewer failed (exit {result}); inspect {log_path}. Existing sessions and sockets are preserved.", file=sys.stderr)
            return result if result >= 0 else 1
        with process.stdout:
            for line in iter(process.stdout.readline, b""):
                log.write(line)
                sys.stdout.buffer.write(line)
                sys.stdout.buffer.flush()
        result = process.wait()
        log.write(f"Viewer exit code: {result}\n".encode())
    if result:
        print(f"Viewer failed (exit {result}); inspect {log_path}. "
              "Existing sessions and sockets are preserved.", file=sys.stderr)
    return result if result >= 0 else 1


def ensure_service(sessions,runtime,root):
    service = request(sessions, runtime, root)
    health = request(sessions, runtime, root, "health")
    if "workspace.create_at" not in health.get("methods",[]):
        print("Updating the organization service; existing shells stay running.",flush=True)
        request(sessions,runtime,root,"service.shutdown")
        deadline=time.monotonic()+5
        while True:
            try:
                current=exchange(sessions,dict(protocol=PROTOCOL,method="health"))
                if "workspace.create_at" in current.get("methods",[]): break
            except (Unavailable,ConnectionResetError,BrokenPipeError):
                break
            if time.monotonic()>deadline: raise ValueError("Previous session service did not stop")
            time.sleep(.02)
        service=request(sessions,runtime,root)
        health=request(sessions,runtime,root,"health")
    return service,health


def main():
    runtime = ROOT / "target/debug/workbench-session"
    viewer = ROOT / ".local/build/viewer/ghostling"
    sessions = ROOT / ".local/sessions"
    if not runtime.exists() or not viewer.exists():
        print("Build the spike first with: just spike-build", file=sys.stderr)
        return 1
    try:
        service,health=ensure_service(sessions,runtime,ROOT)
        stopped = sum(terminal["state"] != "running" for terminal in health["terminals"])
        print(f"Session service: {service['service_pid']}; {len(health['terminals']) - stopped} running, {stopped} stopped/unavailable", flush=True)
        if stopped:
            print("Saved terminals were preserved. Use just session-status and just recover-terminal ID to start a replacement shell.", flush=True)
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"Workspace startup failed: {error}. Existing state and sockets are preserved.", file=sys.stderr)
        return 1
    print("Opening saved workspaces. Close detaches shells; exit stops only the focused shell.", flush=True)
    environment = dict(os.environ, WORKBENCH_WORKSPACES=str(sessions / "workspaces.tsv"),
        WORKBENCH_CATALOG_SCRIPT=str(ROOT / "scripts/workspace_catalog.py"),
        WORKBENCH_STATE_DIR=str(sessions), WORKBENCH_RUNTIME=str(runtime), WORKBENCH_PROJECT=str(ROOT))
    environment.pop("WORKBENCH_SOCKET", None)
    try:
        return launch_viewer(viewer, ROOT, sessions, environment, detach=inside_workbench(os.environ))
    except (OSError, ValueError) as error:
        print(f"Viewer startup/logging failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
