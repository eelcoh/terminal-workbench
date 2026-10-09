#!/usr/bin/env python3
"""Bootstrap persistent workspace sessions and attach the experimental viewer."""
from pathlib import Path
import os
import subprocess
import sys
from workspace_catalog import manage

ROOT = Path(__file__).resolve().parent.parent
runtime = ROOT / "target/debug/workbench-session"
viewer = ROOT / ".local/build/viewer/ghostling"
sessions = ROOT / ".local/sessions"
if not runtime.exists() or not viewer.exists():
    sys.exit("Build the spike first with: just spike-build")
try:
    manage(sessions, runtime, ROOT)
except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
    sys.exit(f"Workspace startup failed: {error}. Existing state and sockets are preserved.")
print("Opening saved workspaces. Close detaches shells; exit stops only the focused shell.", flush=True)
environment = dict(os.environ, WORKBENCH_WORKSPACES=str(sessions / "workspaces.tsv"),
    WORKBENCH_CATALOG_SCRIPT=str(ROOT / "scripts/workspace_catalog.py"),
    WORKBENCH_STATE_DIR=str(sessions), WORKBENCH_RUNTIME=str(runtime), WORKBENCH_PROJECT=str(ROOT))
environment.pop("WORKBENCH_SOCKET", None)
result = subprocess.run([str(viewer)], cwd=ROOT, env=environment)
if result.returncode:
    print(f"Attachment failed; inspect logs in {sessions}. Existing sockets are never removed automatically.", file=sys.stderr)
sys.exit(result.returncode)
