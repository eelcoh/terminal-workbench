#!/usr/bin/env python3
"""Real service lifecycle, concurrent clients, adoption and explicit recovery."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, send, wait_until
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from session_service import request, endpoint, exchange, Unavailable, ReplyClosed


def main():
    runtime = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="wb-service-") as temporary:
        root = Path(temporary).resolve()
        state = root / "sessions"
        data = None
        connection = None
        os.environ["SHELL"] = "/bin/sh"
        def call(method="initialize", identity="", title=""):
            return request(state, runtime, root, method, identity, title)
        def marker(name):
            return wait_until(lambda: (root / name).exists() and (root / name).read_text().strip())
        def stopped():
            try:
                exchange(state, dict(protocol=1, method="health", runtime=str(runtime), cwd=str(root)))
                return False
            # A dying service may accept the connection and close it without replying.
            except (Unavailable, ReplyClosed, ConnectionResetError, BrokenPipeError):
                return True
        def stop_service():
            call("service.shutdown")
            wait_until(stopped)
        try:
            try:request(state,runtime,root,"catalog.read")
            except Unavailable:pass
            else:raise AssertionError("catalog read started an absent service")
            assert not state.exists()
            # Simultaneous first windows start exactly one service and one set of shells.
            with ThreadPoolExecutor(max_workers=4) as pool:
                replies = list(pool.map(lambda _: call(), range(4)))
            assert len({reply["service_pid"] for reply in replies}) == 1
            service_pid = replies[0]["service_pid"]
            data = replies[0]["data"]
            assert all(reply["data"] == data for reply in replies)
            status = call("health")
            assert len(status["terminals"]) == 4
            assert all(t["state"] == "running" and "transfer" in t["runtime"]["capabilities"] for t in status["terminals"])
            assert endpoint(state).stat().st_mode & 0o777 == 0o600
            path = state / "spike.sock"
            inode = path.stat().st_ino
            connection, _, _ = attach(path)
            send(connection, 16, b'keep=retained; echo $$ > before.pid\n')
            pid = marker("before.pid")
            # Catalog reads do not rewrite metadata or affect an attached PTY.
            saved=(state/'workspaces.json').read_bytes();stamp=(state/'workspaces.json').stat().st_mtime_ns
            assert exchange(state,dict(protocol=1,method="catalog.read"))["data"]==data
            assert "catalog.read" in status["methods"]
            assert (state/'workspaces.json').read_bytes()==saved and (state/'workspaces.json').stat().st_mtime_ns==stamp
            # One-terminal status is readonly, including with a live attachment.
            assert call("terminal.status","spike")["state"]=="running"
            assert request(state,runtime,root/'other',"terminal.status","spike")["state"]=="running"
            assert path.stat().st_ino==inode
            try:call("terminal.status","missing")
            except ValueError as error:assert "no longer exists" in str(error)
            else:raise AssertionError('unknown identity status accepted')
            appearance = state / "appearance.state"
            appearance.write_bytes(b"saved colors")
            # A configuration mismatch cannot mutate state, but health and an
            # explicit stop remain usable so the caller can change configuration.
            try: request(state, runtime, root / "other", "initialize")
            except ValueError as error: assert "configuration differs" in str(error)
            else: raise AssertionError("changed service configuration accepted")
            assert request(state, runtime, root / "other", "health")["directory"] == str(root)
            # Lifecycle requests do not seize input ownership or lose process state.
            request(state, runtime, root / "other", "service.shutdown")
            wait_until(stopped)
            # A status read never restarts a stopped service.
            try: exchange(state, dict(protocol=1, method="terminal.status", identity="spike"))
            except Unavailable: pass
            else: raise AssertionError("status read reached a stopped service")
            assert not endpoint(state).exists()
            send(connection, 16, b'printf "%s:%s" "$$" "$keep" > service-stopped\n')
            assert marker("service-stopped") == pid + ":retained"
            adopted = call()
            assert adopted["service_pid"] != service_pid and adopted["data"] == data
            assert path.stat().st_ino == inode and appearance.read_bytes() == b"saved colors"
            try:
                call("terminal.recover", "spike")
            except ValueError as error:
                assert "still running" in str(error)
            else:
                raise AssertionError("live runtime recovery accepted")
            send(connection, 16, b'printf "%s:%s" "$$" "$keep" > adopted\n')
            assert marker("adopted") == pid + ":retained"
            # Abrupt service loss leaves a stale endpoint; next client replaces only it.
            crashed_pid = adopted["service_pid"]
            os.kill(crashed_pid, signal.SIGKILL)
            wait_until(stopped)
            assert call()["service_pid"] != crashed_pid
            assert path.stat().st_ino == inode
            send(connection, 16, b'printf "%s:%s" "$$" "$keep" > after-crash\n')
            assert marker("after-crash") == pid + ":retained"
            # Exiting a terminal is not interpreted as permission to restart it.
            connection.close(); connection = None
            dead = state / "main-2.sock"
            subprocess.run([str(runtime), "stop", str(dead)], check=True, timeout=15)
            wait_until(lambda: not dead.exists())
            assert call("terminal.status","main-2")["state"]=="stopped"
            # Uncertain endpoints do not authorize a fresh shell.
            control=dead.with_suffix('.ctl');control.write_text('keep this file')
            assert call("terminal.status","main-2")["state"]=="unavailable"
            assert control.read_text()=='keep this file';control.unlink()
            saved = (state / "workspaces.json").read_bytes()
            assert call()["data"] == data and not dead.exists()
            health = call("health")
            assert next(t for t in health["terminals"] if t["id"] == "main-2")["state"] == "stopped"
            stop_service(); assert call()["data"] == data and not dead.exists()
            # Explicit single-terminal recovery preserves siblings, identity and themes.
            call("terminal.recover", "main-2")
            connection, _, _ = attach(dead)
            send(connection, 16, b'echo recovered > recovery\n')
            assert marker("recovery") == "recovered"
            connection.close(); connection = None
            assert (state / "workspaces.json").read_bytes() == saved
            assert appearance.read_bytes() == b"saved colors" and path.stat().st_ino == inode
            # Requests are serialized; every creator receives its own locked identity.
            with ThreadPoolExecutor(max_workers=3) as pool:
                created = list(pool.map(lambda i: call("workspace.create", title=f"Project {i}"), range(3)))
            identities = [reply["data"]["workspaces"][-1]["id"] for reply in created]
            assert len(set(identities)) == 3
            data = call()["data"]
            assert len(data["workspaces"]) == 5
            # Malformed/unsupported clients fail locally; subsequent health still works.
            for payload in (b'{"protocol":99}\n', b'[]\n', b'not json\n', b"x" * (1024 * 1024 + 1)):
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
                    peer.settimeout(5); peer.connect(str(endpoint(state)))
                    try: peer.sendall(payload); reply = peer.recv(4096)
                    except (ConnectionResetError, BrokenPipeError): reply = b''
                    assert not reply or b'"ok":false' in reply
                assert call("health")["service_pid"] > 0
            # Service endpoint recovery refuses to remove ordinary files or symlinks.
            stop_service()
            endpoint(state).write_bytes(b"leave me")
            try: call()
            except (ValueError, OSError): pass
            else: raise AssertionError("ordinary service endpoint replaced")
            assert endpoint(state).read_bytes() == b"leave me"
            endpoint(state).unlink()
            endpoint(state).symlink_to(appearance)
            try: call()
            except (ValueError, OSError): pass
            else: raise AssertionError("symlink service endpoint replaced")
            assert endpoint(state).is_symlink() and appearance.read_bytes() == b"saved colors"
            endpoint(state).unlink()
            # An unadvertised live endpoint stays unadvertised, not guessed current.
            probe = root / "capability"; probe.mkdir(mode=0o700)
            (probe / "workspaces.json").write_text(json.dumps({"version": 1, "workspaces": [
                {"id": "w", "name": "W", "terminals": [{"id": "legacy", "name": "Legacy"}]}]}))
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(str(probe / "legacy.ctl")); listener.listen()
                import threading
                def reject():
                    listener.settimeout(5)
                    try:
                        while True:
                            peer, _ = listener.accept()
                            with peer: peer.recv(128)
                    except OSError: pass
                thread = threading.Thread(target=reject, daemon=True); thread.start()
                try:
                    health = request(probe, runtime, root, "health")
                    legacy = next(t for t in health["terminals"] if t["id"] == "legacy")
                    assert legacy["runtime"]["compatibility"] == "unadvertised", legacy
                finally:
                    request(probe, runtime, root, "service.shutdown")
            # A lost reply never replays a possibly committed mutation.
            lost = root / "lost-reply"; lost.mkdir()
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(str(endpoint(lost))); listener.listen(); listener.settimeout(3)
                received = []
                def discard_reply():
                    peer, _ = listener.accept()
                    peer.settimeout(3)
                    with peer: received.append(peer.recv(4096))
                worker = threading.Thread(target=discard_reply, daemon=True); worker.start()
                try: request(lost, runtime, root, "workspace.create", title="Once")
                except ValueError as error: assert "closed before replying" in str(error)
                except (ConnectionResetError, BrokenPipeError):
                    # Closing a peer without replying may produce EOF or reset.
                    # Both are indeterminate mutation outcomes, never retries.
                    pass
                else: raise AssertionError("lost mutation reply accepted")
                worker.join(timeout=3)
                assert len(received) == 1 and not (lost / "service.log").exists()
            # The service endpoint also fits the previously reported Mac directory.
            long = root / ("s" * (71 - len(os.fsencode(root)) - 1))
            long.mkdir(); assert len(os.fsencode(long)) == 71
            try:
                empty = request(long, runtime, root, "health")
                assert empty["terminals"] == [] and not (long / "workspaces.json").exists()
                assert len(os.fsencode(endpoint(long))) < 104
            finally:
                request(long, runtime, root, "service.shutdown")
            print("PASS: single service under concurrent launch, capabilities/health, live adoption and crash recovery, stopped identities retained, explicit recovery, sibling PID/state/themes, concurrent mutations, lost-reply isolation, Mac-length paths and malformed/unsafe endpoints")
        finally:
            if connection is not None: connection.close()
            for path in state.glob("*.sock"):
                if path.name != "service.sock":
                    subprocess.run([str(runtime), "stop", str(path)], capture_output=True, timeout=15)
            if endpoint(state).exists() and not endpoint(state).is_symlink():
                try: stop_service()
                except (ValueError, OSError): pass

if __name__ == "__main__":
    main()
