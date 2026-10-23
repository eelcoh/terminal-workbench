#!/usr/bin/env python3
"""Recover catalogs interrupted at each transaction boundary through the real service.

Each case writes the journal a crashed writer would leave (identities committed
before launch, close intent before stops) with the matching live runtimes.
"""
from pathlib import Path
import copy
import json
import os
import subprocess
import sys
import tempfile
from session_smoke import attach, send, wait_until

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import catalog_files
from session_service import request

BASE = {"version": 1, "workspaces": [
    {"id": "target", "name": "Target", "terminals": [{"id": "a", "name": "A"}, {"id": "b", "name": "B"}]},
    {"id": "sibling", "name": "Sibling", "terminals": [{"id": "c", "name": "C"}]}]}
SEED = {"version": 1, "workspaces": [
    {"id": "workspace-0", "name": "Main", "terminals": [{"id": "spike", "name": "Shell 1"}, {"id": "main-2", "name": "Shell 2"}]},
    {"id": "workspace-1", "name": "Scratch", "terminals": [{"id": "scratch-1", "name": "Shell 1"}, {"id": "scratch-2", "name": "Shell 2"}]}]}


def with_terminal(data):
    data = copy.deepcopy(data); data["workspaces"][0]["terminals"].append({"id": "n", "name": "Interrupted"}); return data


def with_workspace(data):
    data = copy.deepcopy(data)
    data["workspaces"].append({"id": "new", "name": "Interrupted", "terminals": [{"id": "n", "name": "Shell 1"}]})
    return data


def cases():
    """(label, catalog left on disk, terminals running, terminals stopped after the crash)."""
    close = dict(copy.deepcopy(BASE), pending_close={"terminals": ["a", "b"], "workspaces": ["target"]})
    return [
        ("initialize first-spawn", dict(copy.deepcopy(SEED), pending_create=["spike", "main-2", "scratch-1", "scratch-2"]), ["spike"], []),
        ("terminal.create intent", dict(with_terminal(BASE), pending_create=["n"]), ["a", "b", "c"], []),
        ("terminal.create first-spawn", dict(with_terminal(BASE), pending_create=["n"]), ["a", "b", "c", "n"], []),
        ("workspace.create first-spawn", dict(with_workspace(BASE), pending_create=["n"]), ["a", "b", "c", "n"], []),
        ("terminal.create committed", with_terminal(BASE), ["a", "b", "c", "n"], []),
        ("workspace.close close-intent", close, ["a", "b", "c"], []),
        ("workspace.close first-stop", close, ["a", "b", "c"], ["a"]),
    ]


def main():
    runtime = Path(sys.argv[1]).resolve()
    os.environ["SHELL"] = "/bin/sh"

    def spawn(state, cwd, identities):
        for identity in identities:
            with (state / "fixture.log").open("ab") as log:
                subprocess.Popen([str(runtime), str(state / (identity + ".sock")), "/bin/sh"], cwd=cwd,
                                 stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
        for identity in identities:
            receipt = state / (identity + ".lifecycle")
            wait_until(lambda: receipt.exists() and json.loads(receipt.read_text()).get("event") == "running")

    def cleanup(state, cwd):
        for path in state.glob("*.sock"):
            subprocess.run([str(runtime), "stop", str(path)], capture_output=True, timeout=15)
        try: request(state, runtime, cwd, "service.shutdown")
        except (ValueError, OSError): pass

    with tempfile.TemporaryDirectory(prefix="wb-journal-") as temporary:
        # The service canonicalizes state paths (Mac /tmp -> /private/tmp).
        root = Path(temporary).resolve()
        for index, (label, intent, running, stopped_first) in enumerate(cases()):
            cwd = root / str(index); cwd.mkdir()
            state = cwd / "s"; state.mkdir(mode=0o700)
            peer = None
            try:
                catalog_files.save(state, intent)
                spawn(state, cwd, running)
                for identity in stopped_first:
                    subprocess.run([str(runtime), "stop", str(state / (identity + ".sock"))], check=True, capture_output=True, timeout=15)
                if "c" in running:
                    peer, _, _ = attach(state / "c.sock")
                    send(peer, 16, b'keep=survives; echo $$ > before.pid\n')
                    wait_until(lambda: (cwd / "before.pid").exists())
                    pid = (cwd / "before.pid").read_text().strip()
                    inode = (state / "c.sock").stat().st_ino
                    (state / "appearance.state").write_text("unchanged")
                before = {i: json.loads((state / (i + ".lifecycle")).read_text())["runtime_pid"]
                          for i in running if i not in stopped_first}
                recovered = request(state, runtime, cwd)["data"]
                assert "pending_create" not in recovered and "pending_close" not in recovered
                assert request(state, runtime, cwd)["data"] == recovered
                if "pending_close" in intent:
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
                print("PASS:", label, flush=True)
            finally:
                if peer: peer.close()
                cleanup(state, cwd)
        # A failed intent write cannot start any process or alter existing data.
        state = root / "write-error"; state.mkdir(mode=0o700)
        catalog_files.save(state, {"version": 1, "workspaces": []})
        try:
            request(state, runtime, root)
            before = (state / "workspaces.json").read_bytes()
            state.chmod(0o500)
            try: request(state, runtime, root, "workspace.create", title="Unwritten")
            except ValueError: pass
            else: raise AssertionError("failed intent write succeeded")
            finally: state.chmod(0o700)
            assert (state / "workspaces.json").read_bytes() == before and not list(state.glob("*.sock"))
            print("PASS: failed-intent isolation")
        finally:
            cleanup(state, root)
        # Malformed intents must fail before any process or filesystem mutation.
        state = root / "bad"; state.mkdir(mode=0o700)
        base = {"version": 1, "workspaces": [{"id": "w", "name": "W", "terminals": [{"id": "t", "name": "T"}]}]}
        try:
            for fields in ({"pending_create": ["missing"]}, {"pending_create": ["t", "t"]}, {"pending_create": []},
                           {"pending_create": [None]}, {"pending_close": {"terminals": [], "workspaces": ["w"]}},
                           {"pending_close": {"terminals": ["t", "t"], "workspaces": []}},
                           {"pending_close": {"terminals": [], "workspaces": []}},
                           {"pending_close": {"terminals": ["t"], "workspaces": []}, "pending_create": ["t"]}):
                data = copy.deepcopy(base); data.update(fields)
                text = json.dumps(data); (state / "workspaces.json").write_text(text)
                try: request(state, runtime, root)
                except ValueError: pass
                else: raise AssertionError("invalid intent accepted")
                assert (state / "workspaces.json").read_text() == text and not list(state.glob("*.sock"))
            print("PASS: invalid intent isolation and complete workspace-close validation")
        finally:
            cleanup(state, root)


if __name__ == "__main__":
    main()
