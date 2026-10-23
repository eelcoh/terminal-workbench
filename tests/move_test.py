#!/usr/bin/env python3
"""Moves preserve identities and reject invalid operations before saving."""
from pathlib import Path
import importlib.util
import tempfile
from unittest.mock import patch
ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('catalog', ROOT / 'scripts/workspace_catalog.py')
catalog = importlib.util.module_from_spec(spec); spec.loader.exec_module(catalog)
def main():
    data = {'version': 1, 'workspaces': [
        {'id': 'a', 'name': 'Source', 'terminals': [{'id': 'one', 'name': 'One'}, {'id': 'two', 'name': 'Two'}]},
        {'id': 'b', 'name': 'Empty', 'terminals': []},
        {'id': 'c', 'name': 'Full', 'terminals': [{'id': f'f{i}', 'name': f'Full {i}'} for i in range(8)]}]}
    with tempfile.TemporaryDirectory(prefix='wb-move-') as directory:
        state = Path(directory); catalog.save(state, data)
        with patch.object(catalog, 'ensure_sessions') as spawn, patch.object(catalog.subprocess, 'run') as runtime:
            moved = catalog.manage(state, Path('runtime'), state, 'terminal.move', 'one', '', 'b', 0)
            assert moved['workspaces'][1]['terminals'] == [{'id': 'one', 'name': 'One'}]
            assert catalog.socket_path(state, moved['workspaces'][1]['terminals'][0]) == state / 'one.sock'
            moved = catalog.manage(state, Path('runtime'), state, 'terminal.move', 'two', '', 'b', 0)
            assert moved['workspaces'][0]['terminals'] == []
            assert [t['id'] for t in moved['workspaces'][1]['terminals']] == ['two', 'one']
            moved = catalog.manage(state, Path('runtime'), state, 'terminal.move', 'two', '', 'b', -1)
            assert [t['id'] for t in moved['workspaces'][1]['terminals']] == ['one', 'two']
            before = (state / 'workspaces.json').read_bytes()
            for identity, destination, position in [('missing', 'a', 0), ('one', 'missing', 0), ('one', 'c', 0), ('one', 'a', 1), ('one', 'a', -2), ('one', 'a', -1.0), ('one', 'a', True), ('one', 'a', '0')]:
                try:
                    catalog.manage(state, Path('runtime'), state, 'terminal.move', identity, '', destination, position)
                    raise AssertionError('invalid move accepted')
                except ValueError: pass
                assert (state / 'workspaces.json').read_bytes() == before
            spawn.assert_not_called(); runtime.assert_not_called()
        assert catalog.load(state) == moved
        # Same-workspace reorder in a full workspace still succeeds.
        catalog.move_terminal(moved, 'f0', 'c', 7)
        assert moved['workspaces'][2]['terminals'][-1]['id'] == 'f0'
        catalog.move_terminal(moved, 'f0', 'c', 0)
        assert moved['workspaces'][2]['terminals'][0]['id'] == 'f0'
    print('PASS: identity-preserving moves, empty sources, ordering, full-workspace reorder, invalid moves and no runtime side effects')
if __name__ == '__main__': main()
