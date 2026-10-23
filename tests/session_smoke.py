#!/usr/bin/env python3
"""Black-box PTY tests: real process, socket clients, resize and reattachment."""
from pathlib import Path
import os
import re
import socket
import signal
import struct
import subprocess
import sys
import tempfile
import time

def wait_until(predicate, timeout=5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        result = predicate()
        if result:
            return result
        time.sleep(0.02)
    raise AssertionError("timed out waiting for test condition")

def send(connection, kind, payload):
    connection.sendall(struct.pack("!BI", kind, len(payload)) + payload)

def read_frame(connection):
    def exact(length):
        data = bytearray()
        while len(data) < length:
            chunk = connection.recv(length - len(data))
            if not chunk:
                raise EOFError("session disconnected")
            data.extend(chunk)
        return bytes(data)
    kind, length = struct.unpack("!BI", exact(5))
    assert length <= 65536
    return kind, exact(length)

def attach(path):
    # bind publishes the path before listen is ready; path.exists is not readiness.
    deadline = time.monotonic() + 5
    while True:
        connection = socket.socket(socket.AF_UNIX)
        connection.settimeout(5)
        try:
            connection.connect(str(path))
            break
        except ConnectionRefusedError:
            connection.close()
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.02)
    output = bytearray()
    sizes = []
    while True:
        kind, payload = read_frame(connection)
        assert kind != 4, payload.decode()
        if kind == 1:
            output.extend(payload)
        elif kind == 2:
            sizes.append(struct.unpack("!HH", payload))
        elif kind == 3:
            return connection, output, sizes

def expect(connection, pattern):
    output = bytearray()
    while True:
        kind, payload = read_frame(connection)
        if kind == 1:
            output.extend(payload)
            match = re.search(pattern, output)
            if match:
                return match

def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="band-session-test-") as directory:
        path = Path(directory) / "session.sock"
        with (Path(directory) / "server.log").open("w+") as log:
            server = subprocess.Popen([str(binary), str(path), "/bin/sh"], stderr=log, env=dict(os.environ, HERDR_WORKSPACE_ID="inherited-test-workspace", HERDR_SOCKET_PATH="inherited.sock"))
            connection = None
            try:
                wait_until(path.exists)
                assert path.stat().st_mode & 0o777 == 0o600
                duplicate = subprocess.run([str(binary), str(path), "/bin/sh"], capture_output=True, timeout=5)
                assert duplicate.returncode != 0 and path.exists(), "duplicate server must not unlink the live socket"
                connection, _, sizes = attach(path)
                assert sizes[0] == (80, 24)
                send(connection, 16, b"keep=retained; printf '\\nWB_PID:%s\\n' \"$$\"\n")
                pid = expect(connection, rb"\r?\nWB_PID:(\d+)\r?\n").group(1)
                send(connection, 16, b"printf '\\nWB_ENV:%s:%s\\n' \"${HERDR_WORKSPACE_ID-unset}\" \"${HERDR_SOCKET_PATH-unset}\"\n")
                expect(connection, rb"\r?\nWB_ENV:unset:unset\r?\n")
                extra = socket.socket(socket.AF_UNIX)
                extra.settimeout(5)
                extra.connect(str(path))
                assert read_frame(extra)[0] == 4, "a second attached viewer must be rejected"
                extra.close()
                send(connection, 17, struct.pack("!HH", 93, 31))
                send(connection, 16, b"printf '\\nWB_SIZE:'; stty size\n")
                expect(connection, rb"\r?\nWB_SIZE:31 93\r?\n")
                send(connection, 16, b"printf '\\033[?1049h\\033[2JALT_SCREEN\\n'; sleep 0.2; printf 'DETACHED_OUTPUT\\n'\n")
                expect(connection, rb"\x1b\[2JALT_SCREEN")
                connection.close()
                connection = None
                time.sleep(0.3)
                os.kill(int(pid), 0)
                assert server.poll() is None, "closing the viewer must preserve the runtime"
                connection, replay, sizes = attach(path)
                assert b"\x1b[?1049h" in replay and b"DETACHED_OUTPUT\r\n" in replay
                assert sizes == [(80, 24), (93, 31)]
                send(connection, 16, b"printf '\\033[?1049l\\nWB_REATTACH:%s:%s\\n' \"$$\" \"$keep\"\n")
                match = expect(connection, rb"\r?\nWB_REATTACH:(\d+):retained\r?\n")
                assert match.group(1) == pid
                # Malformed input disconnects only the viewer, preserving the shell.
                send(connection, 99, b"")
                try:
                    while connection.recv(4096):
                        pass
                except ConnectionResetError:
                    pass
                connection.close()
                connection = None
                time.sleep(0.05)
                connection, _, _ = attach(path)
                send(connection, 16, b"exit\n")
                assert server.wait(timeout=5) == 0
                assert not path.exists(), "clean exit removes only its own socket"
                print("PASS: binary transport, socket ownership, input, resize, detached output, same-PID reattach, alternate-screen replay, malformed-client isolation")
            finally:
                if connection:
                    connection.close()
                if server.poll() is None:
                    server.terminate()
                    server.wait(timeout=5)
                log.seek(0)
                if sys.exc_info()[0]:
                    print(log.read(), file=sys.stderr)
    # A truncated VT journal must never be replayed as an authoritative screen.
    with tempfile.TemporaryDirectory(prefix="workbench-cap-test-") as directory:
        root = Path(directory)
        path = root / "session.sock"
        with (root / "server.log").open("w+") as log:
            server = subprocess.Popen([str(binary), str(path), "/bin/sh"], cwd=root, stderr=log)
            connection = None
            try:
                wait_until(path.exists)
                connection, _, _ = attach(path)
                send(connection, 16, b"printf '\\nWB_CAP_PID:%s\\n' \"$$\"\n")
                pid = int(expect(connection, rb"\r?\nWB_CAP_PID:(\d+)\r?\n").group(1))
                send(connection, 16, b"head -c 17825792 /dev/zero; printf '\\nWB_CAP_STATE\\n'; printf done > cap.done\n")
                connection.close()
                connection = None
                wait_until(lambda: (root / "cap.done").exists(), timeout=10)
                os.kill(pid, 0)
                connection = socket.socket(socket.AF_UNIX)
                connection.settimeout(5)
                connection.connect(str(path))
                kind, payload = read_frame(connection)
                assert kind == 4 and b"16 MiB" in payload
                wait_until(lambda: b"WB_CAP_STATE" in subprocess.check_output([str(binary), "read", str(path)], timeout=5))
                exported = root / "cap.snapshot"
                subprocess.run([str(binary), "snapshot", str(path), str(exported)], check=True, timeout=5)
                assert exported.read_bytes().startswith(b"GHOSTSNP")
                os.kill(pid, signal.SIGHUP)
                assert server.wait(timeout=5) == 0
                assert not path.exists()
                print("PASS: legacy replay cap preserves live shell and runtime text/binary snapshots beyond 16 MiB")
            finally:
                if connection:
                    connection.close()
                if server.poll() is None:
                    server.terminate()
                    server.wait(timeout=5)
                if sys.exc_info()[0]:
                    log.seek(0)
                    print(log.read(), file=sys.stderr)

if __name__ == "__main__":
    main()
