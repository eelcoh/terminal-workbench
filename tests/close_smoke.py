#!/usr/bin/env python3
"""Stop is independent of viewer attachment and survives malformed control input."""
from pathlib import Path
import os
import socket
import subprocess
import sys
import tempfile
from session_smoke import attach, send, wait_until

def main():
    runtime = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="wb-stop-") as directory:
        root = Path(directory)
        path = root / "session.sock"
        with (root / "runtime.log").open("w+") as log:
            process = subprocess.Popen([runtime, str(path), "/bin/sh"], cwd=root, stdout=log, stderr=log)
            viewer = None
            try:
                wait_until(lambda: path.with_suffix(".ctl").exists())
                assert path.with_suffix(".ctl").stat().st_mode & 0o777 == 0o600
                bad = socket.socket(socket.AF_UNIX)
                bad.connect(str(path.with_suffix(".ctl")))
                bad.sendall(b"WRONG")
                bad.close()
                viewer, _, _ = attach(path)
                send(viewer, 16, b"trap '' HUP; printf '%s' \"$$\" > shell.pid\n")
                wait_until(lambda: (root / "shell.pid").exists())
                shell_pid = int((root / "shell.pid").read_text())
                subprocess.run([runtime, "stop", str(path)], check=True, capture_output=True, timeout=12)
                assert process.wait(timeout=5) == 0
                assert not path.exists() and not path.with_suffix(".ctl").exists()
                try:
                    os.kill(shell_pid, 0)
                    raise AssertionError("stopped shell is alive")
                except ProcessLookupError:
                    pass
                subprocess.run([runtime, "stop", str(path)], check=True, timeout=3)
            finally:
                if viewer: viewer.close()
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    print("PASS: explicit stop with attached viewer, malformed request isolation, signal-ignoring shell, private endpoint, cleanup and repeated stop")

if __name__ == "__main__": main()
