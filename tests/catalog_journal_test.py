#!/usr/bin/env python3
"""Kill the catalog writer at transaction boundaries, then use the real service."""
from pathlib import Path
import copy
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
from unittest.mock import patch
from session_smoke import attach, send, wait_until

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import workspace_catalog as catalog
from session_service import request


def worker(runtime, state, cwd, method, identity, boundary):
    # Test-only interception: no crash controls or environment hooks in production.
    write = catalog.atomic_write
    run = catalog.subprocess.run
    def pause():
        (cwd / "paused").write_text("ready")
        while True:
            time.sleep(0.1)
    def write_boundary(path, value):
        write(path, value)
        if path.name == "workspaces.json":
            data = json.loads(value)
            if boundary == "intent" and "pending_create" in data:
                pause()
            if boundary == "committed" and "pending_create" not in data:
                pause()
        if boundary == "close-intent" and path.name == "workspaces.tsv":
            pause()
    def stop_boundary(*args, **kwargs):
        result = run(*args, **kwargs)
        if boundary == "first-stop" and result.returncode == 0:
            pause()
        return result
    def spawned(identity, process):
        if boundary == "first-spawn":
            pause()
    with patch.object(catalog, "atomic_write", side_effect=write_boundary), patch.object(catalog.subprocess, "run", side_effect=stop_boundary):
        catalog.manage(state, runtime, cwd, method, identity, "Interrupted", on_spawn=spawned)
    raise AssertionError("missed crash boundary")


def main():
    runtime = Path(sys.argv[1]).resolve()
    os.environ["SHELL"] = "/bin/sh"
    with tempfile.TemporaryDirectory(prefix="wb-journal-") as temporary:
        root = Path(temporary)
        # Initial partial seed, new terminal before/after launch, workspace and
        # metadata committed before projection: all retain stable identities.
        cases = [("initialize", "first-spawn"), ("terminal.create", "intent"),
                 ("terminal.create", "first-spawn"), ("workspace.create", "first-spawn"),
                 ("terminal.create", "committed"), ("workspace.close", "close-intent"),
                 ("workspace.close", "first-stop")]
        for index, (method, boundary) in enumerate(cases):
            cwd = root / str(index); cwd.mkdir()
            state = cwd / "s"; state.mkdir(mode=0o700)
            peer = None
            process = None
            try:
                if method != "initialize":
                    # A populated target and independent sibling, all real shells.
                    data = {"version": 1, "workspaces": [
                        {"id": "target", "name": "Target", "terminals": [
                            {"id": "a", "name": "A"}, {"id": "b", "name": "B"}]},
                        {"id": "sibling", "name": "Sibling", "terminals": [{"id": "c", "name": "C"}]}]}
                    catalog.save(state, data)
                    catalog.ensure_sessions(state, data, runtime, cwd)
                    peer, _, _ = attach(state / "c.sock")
                    send(peer, 16, b'keep=survives; echo $$ > before.pid\n')
                    wait_until(lambda: (cwd / "before.pid").exists())
                    pid = (cwd / "before.pid").read_text().strip()
                    inode = (state / "c.sock").stat().st_ino
                    (state / "appearance.state").write_text("unchanged")
                process = subprocess.Popen([sys.executable, __file__, "--worker", str(runtime), str(state), str(cwd), method,
                                            "target" if method in {"terminal.create", "workspace.close"} else "", boundary])
                wait_until(lambda: (cwd / "paused").exists())
                intent = catalog.load(state)
                live_ids = [t["id"] for w in intent["workspaces"] for t in w["terminals"]
                            if (state / (t["id"] + ".sock")).exists()]
                for live_id in live_ids:
                    receipt = state / (live_id + ".lifecycle")
                    wait_until(lambda: receipt.exists() and json.loads(receipt.read_text()).get("event") == "running")
                before = {t["id"]: json.loads((state / (t["id"] + ".lifecycle")).read_text())["runtime_pid"]
                          for w in intent["workspaces"] for t in w["terminals"]
                          if (state / (t["id"] + ".sock")).exists()}
                os.kill(process.pid, signal.SIGKILL); process.wait(timeout=5)
                recovered = request(state, runtime, cwd)["data"]
                assert "pending_create" not in recovered and "pending_close" not in recovered
                assert (state / "workspaces.tsv").read_text() == catalog.snapshot(state, recovered)
                assert (state / "terminal-labels.tsv").read_text() == catalog.terminal_labels(recovered)
                assert request(state, runtime, cwd)["data"] == recovered
                if method == "workspace.close":
                    assert {w["id"] for w in recovered["workspaces"]} == {"sibling"}
                    assert not (state / "a.sock").exists() and not (state / "b.sock").exists()
                else:
                    assert recovered["workspaces"] == intent["workspaces"]
                    health = request(state, runtime, cwd, "health")
                    assert "catalog-journal-v1" in health["features"]
                    assert all(t["state"] == ("running" if t["id"] in before else "stopped") for t in health["terminals"])
                    for identity, runtime_pid in before.items():
                        assert json.loads((state / (identity + ".lifecycle")).read_text())["runtime_pid"] == runtime_pid
                    stopped = next((t for t in health["terminals"] if t["state"] == "stopped"), None)
                    if stopped:
                        request(state, runtime, cwd, "terminal.recover", stopped["id"])
                        assert request(state, runtime, cwd, "terminal.status", stopped["id"])["state"] == "running"
                if peer:
                    send(peer, 16, b'printf "%s:%s" "$$" "$keep" > after.pid\n')
                    wait_until(lambda: (cwd / "after.pid").exists())
                    assert (cwd / "after.pid").read_text() == pid + ":survives"
                    assert (state / "c.sock").stat().st_ino == inode
                    assert (state / "appearance.state").read_text() == "unchanged"
                print("PASS:", method, boundary, flush=True)
            finally:
                if process and process.poll() is None:
                    process.kill(); process.wait(timeout=5)
                if peer: peer.close()
                for path in state.glob("*.sock"):
                    subprocess.run([str(runtime), "stop", str(path)], capture_output=True, timeout=15)
                try: request(state, runtime, cwd, "service.shutdown")
                except (ValueError, OSError): pass
        # Ordinary launch errors also retain successful shells and saved intent.
        state = root / "partial"; state.mkdir(mode=0o700)
        def interrupted(identity, process):
            raise ValueError("injected post-launch error")
        try:
            try: catalog.manage(state, runtime, root, on_spawn=interrupted)
            except ValueError as error: assert str(error) == "injected post-launch error"
            else: raise AssertionError("error was swallowed")
            assert "pending_create" in catalog.load(state)
            receipt = state / "spike.lifecycle"
            wait_until(lambda: receipt.exists() and json.loads(receipt.read_text()).get("event") == "running")
            live = json.loads(receipt.read_text())["runtime_pid"]
            request(state, runtime, root)
            health = request(state, runtime, root, "health")
            assert [t["id"] for t in health["terminals"] if t["state"] == "running"] == ["spike"]
            assert json.loads((state / "spike.lifecycle").read_text())["runtime_pid"] == live
        finally:
            for path in state.glob("*.sock"):
                subprocess.run([str(runtime), "stop", str(path)], capture_output=True, timeout=15)
            request(state, runtime, root, "service.shutdown")
        # A failed intent write cannot start any process or alter existing data.
        state = root / "write-error"; state.mkdir()
        catalog.save(state, {"version": 1, "workspaces": []})
        before = (state / "workspaces.json").read_bytes()
        with patch.object(catalog, "atomic_write", side_effect=OSError("disk error")), patch.object(catalog, "ensure_sessions") as start:
            try: catalog.manage(state, runtime, root, "workspace.create", title="Unwritten")
            except OSError: pass
            else: raise AssertionError("failed intent write succeeded")
            start.assert_not_called()
            assert (state / "workspaces.json").read_bytes() == before
        print("PASS: partial launch-error preservation and failed-intent isolation")
        # Malformed intents must fail before any process or filesystem mutation.
        state = root / "bad"; state.mkdir()
        base = {"version": 1, "workspaces": [{"id": "w", "name": "W", "terminals": [{"id": "t", "name": "T"}]}]}
        for fields in ({"pending_create": ["missing"]}, {"pending_create": ["t", "t"]}, {"pending_create": []},
                       {"pending_create": [None]}, {"pending_close": {"terminals": [], "workspaces": ["w"]}},
                       {"pending_close": {"terminals": ["t", "t"], "workspaces": []}},
                       {"pending_close": {"terminals": [], "workspaces": []}},
                       {"pending_close": {"terminals": ["t"], "workspaces": []}, "pending_create": ["t"]}):
            data = copy.deepcopy(base); data.update(fields)
            text = json.dumps(data); (state / "workspaces.json").write_text(text)
            with patch.object(catalog, "ensure_sessions") as start, patch.object(catalog.subprocess, "run") as stop:
                try: catalog.manage(state, runtime, root)
                except ValueError: pass
                else: raise AssertionError("invalid intent accepted")
                start.assert_not_called(); stop.assert_not_called()
                assert (state / "workspaces.json").read_text() == text
        print("PASS: invalid intent isolation and complete workspace-close validation")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--worker":
        worker(*[Path(v) for v in sys.argv[2:5]], *sys.argv[5:8])
    else:
        main()
