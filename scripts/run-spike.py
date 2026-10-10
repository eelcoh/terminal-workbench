#!/usr/bin/env python3
"""Bootstrap persistent workspace sessions and attach the experimental viewer."""
from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess
import sys
from workspace_catalog import manage

ROOT = Path(__file__).resolve().parent.parent


def launch_viewer(viewer, root, sessions, environment):
    """Keep viewer diagnostics separate from persistent shell logs, and tee them."""
    log_path = sessions / "viewer.log"
    print(f"Viewer log: {log_path}", flush=True)
    with log_path.open("ab", buffering=0) as log:
        log.write(f"\n--- Viewer launch {datetime.now(timezone.utc).isoformat()} ---\n{viewer}\n".encode())
        try:
            process = subprocess.Popen([str(viewer)], cwd=root, env=environment,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except OSError as error:
            message = f"Could not launch viewer: {error}\n"
            log.write(message.encode())
            print(message, file=sys.stderr, end="")
            return 1
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


def main():
    runtime = ROOT / "target/debug/workbench-session"
    viewer = ROOT / ".local/build/viewer/ghostling"
    sessions = ROOT / ".local/sessions"
    if not runtime.exists() or not viewer.exists():
        print("Build the spike first with: just spike-build", file=sys.stderr)
        return 1
    try:
        manage(sessions, runtime, ROOT)
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"Workspace startup failed: {error}. Existing state and sockets are preserved.", file=sys.stderr)
        return 1
    print("Opening saved workspaces. Close detaches shells; exit stops only the focused shell.", flush=True)
    environment = dict(os.environ, WORKBENCH_WORKSPACES=str(sessions / "workspaces.tsv"),
        WORKBENCH_CATALOG_SCRIPT=str(ROOT / "scripts/workspace_catalog.py"),
        WORKBENCH_STATE_DIR=str(sessions), WORKBENCH_RUNTIME=str(runtime), WORKBENCH_PROJECT=str(ROOT))
    environment.pop("WORKBENCH_SOCKET", None)
    try:
        return launch_viewer(viewer, ROOT, sessions, environment)
    except OSError as error:
        print(f"Viewer startup/logging failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
