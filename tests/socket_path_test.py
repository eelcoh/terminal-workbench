#!/usr/bin/env python3
"""Creation must fit Mac socket limits where short seeded terminals already run."""
from pathlib import Path
import importlib.util
import os
import subprocess
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
from session_smoke import attach, send, wait_until

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('catalog',ROOT/'scripts/workspace_catalog.py')
catalog=importlib.util.module_from_spec(spec); spec.loader.exec_module(catalog)


def sized_directory(root, size):
    directory=root/('s'*(size-len(os.fsencode(root))-1))
    directory.mkdir()
    assert len(os.fsencode(directory))==size
    return directory


def main():
    runtime=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='wb-path-') as directory:
        root=Path(directory).resolve()
        # The absolute session directory from the Mac report is exactly 71 bytes.
        state=sized_directory(root,71)
        assert len(os.fsencode(state/('a'*32+'.sock')))==109
        env=dict(os.environ,SHELL='/bin/sh')
        data=None
        try:
            with patch.dict(os.environ,env): data=catalog.manage(state,runtime,root)
            before=[(t['id'],catalog.socket_path(state,t).stat().st_ino) for w in data['workspaces'] for t in w['terminals']]
            connection,_,_=attach(state/'spike.sock')
            send(connection,16,b'keep=retained; echo $$ > before.pid\n')
            wait_until(lambda:(root/'before.pid').exists())
            pid=(root/'before.pid').read_bytes()
            connection.close()
            preferences=state/'appearance.state'; preferences.write_bytes(b'preserve settings')
            with patch.dict(os.environ,env):
                data=catalog.manage(state,runtime,root,'workspace.create',title='Mac project')
                data=catalog.manage(state,runtime,root,'terminal.create',identity='workspace-0',title='Mac terminal')
            created=[data['workspaces'][-1]['terminals'][0], data['workspaces'][0]['terminals'][-1]]
            for index,terminal in enumerate(created):
                assert len(terminal['id'])==16
                path=catalog.socket_path(state,terminal)
                assert len(os.fsencode(path))==93
                connection,_,_=attach(path)
                send(connection,16,f'printf working > created-{index}\n'.encode())
                assert wait_until(lambda:(root/f'created-{index}').exists() and (root/f'created-{index}').read_text())=='working'
                connection.close()
            assert catalog.load(state)==data and preferences.read_bytes()==b'preserve settings'
            for identity,inode in before: assert (state/f'{identity}.sock').stat().st_ino==inode
            connection,_,_=attach(state/'spike.sock')
            send(connection,16,b'printf "%s" "$keep" > retained; echo $$ > after.pid\n')
            wait_until(lambda:(root/'after.pid').exists() and (root/'retained').exists())
            assert (root/'after.pid').read_bytes()==pid and (root/'retained').read_text()=='retained'
            connection.close()
            # Identity allocation retries a collision with either kind of entity.
            used=data['workspaces'][0]['terminals'][-1]['id']
            with patch.object(catalog.uuid,'uuid4',side_effect=[SimpleNamespace(hex=used+'0'*16),SimpleNamespace(hex='f'*32)]):
                assert catalog.new_terminal_identity(data)=='f'*16
            with patch.object(catalog.uuid,'uuid4',return_value=SimpleNamespace(hex=used+'0'*16)):
                try: catalog.new_terminal_identity(data)
                except ValueError: pass
                else: raise AssertionError('collision exhaustion accepted')
            # Older 32-character IDs stay unchanged when saved/reloaded/moved.
            old= root/'legacy'; old.mkdir()
            legacy={'version':1,'workspaces':[{'id':'original','name':'Legacy','terminals':[{'id':'a'*32,'name':'Shell'}]}]}
            catalog.save(old,legacy)
            assert catalog.load(old)==legacy and str(old/('a'*32+'.sock')) in (old/'workspaces.tsv').read_text()
            # At the hard byte limit, creation fails before any runtime or file mutation.
            long=sized_directory(root,82)
            with patch.object(catalog,'ensure_sessions',return_value=[]) as spawn:
                catalog.manage(long,runtime,root)
                saved=(long/'workspaces.json').read_bytes(); projection=(long/'workspaces.tsv').read_bytes(); spawn.reset_mock()
                for method,identity in [('workspace.create',''),('terminal.create','workspace-0')]:
                    try: catalog.manage(long,runtime,root,method,identity,'Too long')
                    except ValueError as error: assert '103 bytes' in str(error) and 'shorter checkout' in str(error)
                    else: raise AssertionError('overlong socket accepted')
                    spawn.assert_not_called()
                    assert (long/'workspaces.json').read_bytes()==saved and (long/'workspaces.tsv').read_bytes()==projection
        finally:
            if data:
                for workspace in data['workspaces']:
                    for terminal in workspace['terminals']:
                        subprocess.run([str(runtime),'stop',str(state/f"{terminal['id']}.sock")],capture_output=True,timeout=15)
    print('PASS: real workspace/terminal creation at reported Mac path length, socket/control attachment, existing PID/state/themes retained, collision retry/exhaustion, legacy IDs retained, overlong paths fail before mutation')


if __name__=='__main__': main()
