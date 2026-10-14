#!/usr/bin/env python3
"""Organization persistence and identity checks without GUI or real shells."""
from pathlib import Path
import copy
import importlib.util
import json
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("workspace_catalog", ROOT / "scripts/workspace_catalog.py")
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)

def main():
    with tempfile.TemporaryDirectory(prefix="workbench-catalog-test-") as directory:
        state = Path(directory)
        (state / "workspaces.tsv").write_text(f"Main\t{state}/spike.sock\t{state}/main-2.sock\nScratch\t{state}/scratch-1.sock\t{state}/scratch-2.sock\n")
        with patch.object(catalog, "ensure_sessions", return_value=[]) as spawn:
            original = catalog.manage(state, Path("runtime"), state)
            assert original["workspaces"][0]["terminals"][0]["id"] == "spike"
            workspace_id = original["workspaces"][0]["id"]
            initial_calls = spawn.call_count
            renamed = catalog.manage(state, Path("runtime"), state, "workspace.rename", workspace_id, "Projects")
            renamed_terminal = catalog.manage(state, Path("runtime"), state, "terminal.rename", "spike", "Builder")
            assert spawn.call_count == initial_calls, "rename must not launch runtimes"
            assert renamed["workspaces"][0]["id"] == workspace_id
            assert renamed_terminal["workspaces"][0]["terminals"][0]["id"] == "spike"
            assert json.loads((state / "workspaces.json").read_text()) == renamed_terminal
            assert renamed_terminal['workspaces'][0]['terminals'][0]['custom_name'] is True
            assert 'spike\t1\n' in (state/'terminal-labels.tsv').read_text()
            # Explicitly choosing a generic name still overrides live context.
            catalog.mutate(renamed_terminal,'terminal.rename','spike','Shell 1')
            catalog.save(state,renamed_terminal)
            assert 'spike\t1\n' in (state/'terminal-labels.tsv').read_text()
            assert catalog.custom_terminal_name({'name':'Shell 2'}) is False
            assert catalog.custom_terminal_name({'name':'Builder'}) is True
            created = catalog.manage(state, Path("runtime"), state, "workspace.create", "", "Research")
            assert len(created["workspaces"]) == 3 and len(created["workspaces"][-1]["terminals"]) == 1
            created = catalog.manage(state, Path("runtime"), state, "terminal.create", workspace_id, "Tests")
            assert len(created["workspaces"][0]["terminals"]) == 3
            ids = [item["id"] for w in created["workspaces"] for item in [w, *w["terminals"]]]
            assert len(ids) == len(set(ids))
            assert catalog.load(state) == created
            before = (state / "workspaces.json").read_bytes()
            for method, identity, title in [("terminal.rename", "missing", "Name"), ("workspace.rename", workspace_id, ""), ("workspace.create", "", "bad\nname")]:
                calls = spawn.call_count
                try:
                    catalog.manage(state, Path("runtime"), state, method, identity, title)
                    raise AssertionError("invalid action succeeded")
                except ValueError:
                    pass
                assert (state / "workspaces.json").read_bytes() == before and spawn.call_count == calls
            # Capacity/identity corruption is rejected before runtime mutation.
            bad = copy.deepcopy(created)
            bad["workspaces"][1]["terminals"][0]["id"] = "spike"
            try:
                catalog.validate(bad)
                raise AssertionError("duplicate identity accepted")
            except ValueError:
                pass
            while len(created["workspaces"][0]["terminals"]) < catalog.MAX_TERMINALS:
                catalog.mutate(created, "terminal.create", workspace_id, "Terminal")
            try:
                catalog.mutate(created, "terminal.create", workspace_id, "Too many")
                raise AssertionError("capacity exceeded")
            except ValueError:
                pass
        # A close intent survives interruption, and recovery precedes startup.
        created = catalog.load(state)
        terminal_id = created["workspaces"][0]["terminals"][0]["id"]
        with patch.object(catalog.subprocess, "run", side_effect=OSError("interrupted")):
            try:
                catalog.manage(state, Path("runtime"), state, "terminal.close", terminal_id)
                raise AssertionError("interrupted stop succeeded")
            except OSError:
                pass
        assert catalog.load(state)["pending_close"]["terminals"] == [terminal_id]
        with patch.object(catalog.subprocess, "run") as stop, patch.object(catalog, "ensure_sessions", return_value=[]) as spawn:
            stop.return_value.returncode = 0
            recovered = catalog.manage(state, Path("runtime"), state)
            assert "pending_close" not in recovered
            assert terminal_id not in {t["id"] for w in recovered["workspaces"] for t in w["terminals"]}
            spawn.assert_not_called()  # Initialization never resurrects saved shells.
            for workspace in list(recovered["workspaces"]):
                catalog.manage(state, Path("runtime"), state, "workspace.close", workspace["id"])
            assert catalog.load(state)["workspaces"] == []
            assert (state / "workspaces.tsv").read_text() == "workbench-catalog-v3\n"
        # Earlier runtime endpoints are rejected before any close intent or stop.
        with patch.object(catalog, "ensure_sessions", return_value=[]):
            data = catalog.manage(state, Path("runtime"), state, "workspace.create", "", "Legacy")
        terminal = data["workspaces"][0]["terminals"][0]
        catalog.socket_path(state, terminal).touch()
        before = (state / "workspaces.json").read_bytes()
        with patch.object(catalog.subprocess, "run") as stop:
            try:
                catalog.manage(state, Path("runtime"), state, "workspace.close", data["workspaces"][0]["id"])
                raise AssertionError("legacy session accepted")
            except ValueError:
                pass
            stop.assert_not_called()
            assert (state / "workspaces.json").read_bytes() == before
        # A newer/corrupt metadata document is preserved, never silently reseeded.
        (state / "workspaces.json").write_text('{"version": 99}')
        with patch.object(catalog, "ensure_sessions") as spawn:
            try:
                catalog.manage(state, Path("runtime"), state)
                raise AssertionError("unsupported metadata accepted")
            except ValueError:
                pass
            spawn.assert_not_called()
            assert (state / "workspaces.json").read_text() == '{"version": 99}'
    print("PASS: catalog migration, stable identities, create/rename persistence, invalid-action isolation, capacity, corrupt-state preservation, interrupted-close recovery, legacy rejection and empty catalogs")

if __name__ == "__main__":
    main()
