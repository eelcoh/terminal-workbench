#!/usr/bin/env python3
"""Foreground identity fixtures, subscriptions, detach, and ordinary-shell fallback.
Fixtures are named native executables, not real coding-agent compatibility tests.
"""
from pathlib import Path
import json
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, send, read_frame, wait_until, expect

def snapshot(runtime, path):
    result = subprocess.run([runtime, "inspect", str(path)], check=True, capture_output=True, text=True, timeout=5)
    return json.loads(result.stdout)

def main():
    runtime = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="wb-agent-") as directory:
        root = Path(directory)
        path = root / "session.sock"
        fixture = root / "codex"
        shutil.copyfile("/usr/bin/sleep", fixture)
        fixture.chmod(0o700)
        with (root / "runtime.log").open("w+") as log:
            process = subprocess.Popen([runtime, str(path), "/bin/sh"], cwd=root, stdout=log, stderr=log)
            viewer = watcher = None
            try:
                wait_until(lambda: path.with_suffix(".ctl").exists())
                viewer, _, _ = attach(path)
                wait_until(lambda: snapshot(runtime, path)["available"])
                assert snapshot(runtime, path)["agent"] is None
                # An unsubscribed old viewer receives only the original PTY frames.
                send(viewer, 16, b"printf 'claude codex working waiting\n'\n")
                expect(viewer, b"claude codex working waiting")
                assert snapshot(runtime, path)["agent"] is None
                send(viewer, 16, str(fixture).encode() + b" 120\n")
                wait_until(lambda: snapshot(runtime, path)["agent"] == "codex")
                viewer.settimeout(0.1)
                try:
                    while True:
                        kind, _ = read_frame(viewer)
                        assert kind in (1, 2), "metadata leaked into the legacy PTY stream"
                except socket.timeout:
                    pass
                viewer.settimeout(5)
                watcher = socket.socket(socket.AF_UNIX)
                watcher.settimeout(5)
                watcher.connect(str(path.with_suffix(".ctl")))
                watcher.sendall(b"WATCH")
                assert read_frame(watcher) == (5, b"\x01")
                viewer.close()
                viewer = None
                time.sleep(0.2)
                assert snapshot(runtime, path)["agent"] == "codex", "runtime identification must survive detach"
                viewer, _, _ = attach(path)
                send(viewer, 16, b"\x03")
                assert read_frame(watcher) == (5, b"\x00")
                wait_until(lambda: snapshot(runtime, path)["agent"] is None)
                send(viewer, 16, b"printf same-shell-ready\n")
                expect(viewer, b"same-shell-ready")
                # Dropping a watcher cannot detach the terminal or block Stop.
                watcher.close()
                watcher = None
                subprocess.run([runtime, "stop", str(path)], check=True, capture_output=True, timeout=12)
                assert process.wait(timeout=5) == 0
            finally:
                if viewer: viewer.close()
                if watcher: watcher.close()
                if process.poll() is None:
                    subprocess.run([runtime, "stop", str(path)], timeout=12, capture_output=True)
                    process.wait(timeout=5)
    print("PASS: foreground executable identification, output-text isolation, runtime inspect, optional watch, detach/reattach, identity clearing, watcher isolation and stop")

if __name__ == "__main__": main()
