#!/usr/bin/env python3
"""Explicit selected shutdown retains organization, peers and restart guards."""
from pathlib import Path
import copy
import os
import socket
import subprocess
import sys
import tempfile
import time
from session_smoke import attach, send, wait_until
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import catalog_files
from session_service import request


def main():
    runtime = Path(sys.argv[1]).resolve()
    os.environ["SHELL"] = "/bin/sh"
    with tempfile.TemporaryDirectory(prefix="wb-stop-all-") as temporary:
        root = Path(temporary).resolve()
        def call(state, method="initialize", identity="", targets=None):
            return request(state, runtime, root, method, identity, terminal_ids=targets)
        def cleanup(state):
            for path in state.glob("*.sock"):
                subprocess.run([str(runtime), "stop", str(path)], capture_output=True, timeout=15)
            try: call(state, "service.shutdown")
            except (ValueError, OSError): pass
        for count in (1, 15):
            state = root / str(count); state.mkdir(mode=0o700)
            ids = ["t" + str(i) for i in range(count + 1)]
            data = {"version": 1, "workspaces": [
                {"id": "w" + str(start), "name": "Workspace " + str(start), "terminals": [
                    {"id": i, "name": "Terminal " + i} for i in ids[start:start+8]]}
                for start in range(0, len(ids), 8)]}
            peer = None
            try:
                catalog_files.save(state, data); call(state, "session.recover")
                selected = ids[:-1]  # Snapshot of a confirmation; later IDs are excluded.
                sibling = ids[-1]; peer, _, _ = attach(state / (sibling + ".sock"))
                marker = root / ("pid" + str(count))
                send(peer, 16, ("keep=survives; echo $$ > " + str(marker) + "\n").encode())
                pid = wait_until(lambda: marker.exists() and marker.read_text().strip())
                inode = (state / (sibling + ".sock")).stat().st_ino
                appearance = state / "appearance.state"; appearance.write_bytes(b"keep themes")
                saved = (state / "workspaces.json").read_bytes()
                for invalid in (None, [], selected + selected, selected + ["missing"], [None], selected * 65):
                    try: call(state, "session.stop", targets=invalid)
                    except ValueError: pass
                    else: raise AssertionError("invalid selection accepted")
                    assert (state / "workspaces.json").read_bytes() == saved
                    assert all(t["state"] == "running" for t in call(state, "health")["terminals"])
                start = time.monotonic()
                stopped = call(state, "session.stop", targets=selected)
                duration = time.monotonic() - start
                assert stopped["data"] == data
                assert (state / "workspaces.json").read_bytes() == saved and appearance.read_bytes() == b"keep themes"
                health = call(state, "health")
                assert all(t["state"] == ("running" if t["id"] == sibling else "stopped") for t in health["terminals"])
                assert all(t["lifecycle"]["reason"] == "stopped" for t in health["terminals"] if t["id"] != sibling)
                # Repetition and service/window-style reopening never restart anything.
                call(state, "session.stop", targets=selected)
                call(state, "service.shutdown")
                assert not (state / "service.rpc").exists(), "shutdown replied before retiring its endpoint"
                call(state)
                assert all(not (state / (i + ".sock")).exists() for i in selected)
                send(peer, 16, ("printf '%s:%s' \"$$\" \"$keep\" > " + str(marker) + "after\n").encode())
                assert wait_until(lambda: Path(str(marker)+"after").exists() and Path(str(marker)+"after").read_text()) == pid + ":survives"
                assert (state / (sibling + ".sock")).stat().st_ino == inode
                call(state, "terminal.recover", selected[0])
                assert call(state, "terminal.status", selected[0])["state"] == "running"
                assert (state / "workspaces.json").read_bytes() == saved
                print(f"PASS: {count} selected shutdowns in {duration:.3f}s; retained catalog/themes, excluded sibling and explicit recovery", flush=True)
            finally:
                if peer: peer.close()
                cleanup(state)
        state = root / "interrupted"; state.mkdir(mode=0o700)
        data = {"version": 1, "workspaces": [{"id": "w", "name": "W", "terminals": [
            {"id": i, "name": i} for i in ("a", "b", "c")]}]}
        try:
            # The state a crash leaves after recording intent and stopping one target.
            catalog_files.save(state, data); call(state, "session.recover"); call(state, "service.shutdown")
            subprocess.run([str(runtime), "stop", str(state / "a.sock")], check=True, timeout=15)
            catalog_files.save(state, dict(copy.deepcopy(data), pending_stop=["a", "b"]))
            assert call(state)["data"] == data
            assert call(state, "terminal.status", "a")["state"] == "stopped"
            assert call(state, "terminal.status", "b")["state"] == "stopped"
            assert call(state, "terminal.status", "c")["state"] == "running"
            # Unsafe target aborts before stopping valid peers.
            call(state, "terminal.recover", "a")
            (state / "b.ctl").write_text("preserve")
            try: call(state, "session.stop", targets=["a", "b"])
            except ValueError: pass
            else: raise AssertionError("unsafe endpoint accepted")
            assert call(state, "terminal.status", "a")["state"] == "running"
            assert (state / "b.ctl").read_text() == "preserve"
            (state / "b.ctl").unlink()
            # A listening old replay-only runtime cannot be stopped safely.
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(str(state / "b.sock")); listener.listen()
                try: call(state, "session.stop", targets=["a", "b"])
                except ValueError as error: assert "cannot be stopped" in str(error)
                else: raise AssertionError("legacy runtime accepted")
                assert call(state, "terminal.status", "a")["state"] == "running"
                assert "pending_stop" not in catalog_files.load(state)
                # An individual failed stop retains intent; the next transaction finishes it.
                catalog_files.save(state, dict(copy.deepcopy(data), pending_stop=["a", "b"]))
                try: call(state)
                except ValueError as error: assert "shutdown pending" in str(error)
                else: raise AssertionError("stop failure lost")
                assert catalog_files.load(state)["pending_stop"] == ["a", "b"]
            (state / "b.sock").unlink()
            assert call(state)["data"] == data
            call(state, "terminal.recover", "b")
            assert call(state, "terminal.status", "a")["state"] == "stopped"
            assert call(state, "terminal.status", "b")["state"] == "running"
            assert call(state, "terminal.status", "c")["state"] == "running"
            print("PASS: interrupted and failed shutdown retained/reconciled, no resurrection, unsafe-plan isolation")
        finally:
            cleanup(state)


if __name__ == "__main__":
    main()
