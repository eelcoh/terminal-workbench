#!/usr/bin/env python3
"""Creation must fit Mac socket limits where short seeded terminals already run."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
from session_smoke import attach, send, wait_until

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import catalog_files
from session_service import request


def sized_directory(root, size):
    directory=root/('s'*(size-len(os.fsencode(root))-1))
    directory.mkdir()
    assert len(os.fsencode(directory))==size
    return directory


def main():
    runtime=Path(sys.argv[1]).resolve()
    os.environ['SHELL']='/bin/sh'
    with tempfile.TemporaryDirectory(prefix='wb-path-') as directory:
        root=Path(directory).resolve()
        # The absolute session directory from the Mac report is exactly 71 bytes.
        state=sized_directory(root,71)
        long=sized_directory(root,82)
        assert len(os.fsencode(state/('a'*32+'.sock')))==109
        def call(directory,method='initialize',identity='',title=''):
            return request(directory,runtime,root,method,identity,title)['data']
        try:
            data=call(state)
            before=[(t['id'],catalog_files.socket_path(state,t).stat().st_ino) for t in catalog_files.terminals(data)]
            connection,_,_=attach(state/'spike.sock')
            send(connection,16,b'keep=retained; echo $$ > before.pid\n')
            wait_until(lambda:(root/'before.pid').exists())
            pid=(root/'before.pid').read_bytes()
            connection.close()
            preferences=state/'appearance.state'; preferences.write_bytes(b'preserve settings')
            data=call(state,'workspace.create',title='Mac project')
            data=call(state,'terminal.create','workspace-0','Mac terminal')
            created=[data['workspaces'][-1]['terminals'][0], data['workspaces'][0]['terminals'][-1]]
            for index,terminal in enumerate(created):
                assert len(terminal['id'])==16
                path=catalog_files.socket_path(state,terminal)
                assert len(os.fsencode(path))==93
                connection,_,_=attach(path)
                send(connection,16,f'printf working > created-{index}\n'.encode())
                assert wait_until(lambda:(root/f'created-{index}').exists() and (root/f'created-{index}').read_text())=='working'
                connection.close()
            assert catalog_files.load(state)==data and preferences.read_bytes()==b'preserve settings'
            for identity,inode in before: assert (state/f'{identity}.sock').stat().st_ino==inode
            connection,_,_=attach(state/'spike.sock')
            send(connection,16,b'printf "%s" "$keep" > retained; echo $$ > after.pid\n')
            wait_until(lambda:(root/'after.pid').exists() and (root/'retained').exists())
            assert (root/'after.pid').read_bytes()==pid and (root/'retained').read_text()=='retained'
            connection.close()
            # Older 32-character IDs stay unchanged when loaded through the service.
            old=root/'legacy'; old.mkdir(mode=0o700)
            legacy={'version':1,'workspaces':[{'id':'original','name':'Legacy','terminals':[{'id':'a'*32,'name':'Shell'}]}]}
            catalog_files.save(old,legacy)
            assert call(old)==legacy
            # At the hard byte limit, creation fails before any runtime or file mutation.
            catalog_files.save(long,{'version':1,'workspaces':[{'id':'workspace-0','name':'Main','terminals':[]}]})
            call(long)
            saved=(long/'workspaces.json').read_bytes()
            for method,identity in [('workspace.create',''),('terminal.create','workspace-0')]:
                try: call(long,method,identity,'Too long')
                except ValueError as error: assert '103 bytes' in str(error) and 'shorter checkout' in str(error),error
                else: raise AssertionError('overlong socket accepted')
                assert (long/'workspaces.json').read_bytes()==saved and not list(long.glob('*.sock'))
        finally:
            for directory in (state,long,root/'legacy'):
                for path in directory.glob('*.sock'):
                    subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=15)
                try: request(directory,runtime,root,'service.shutdown')
                except (ValueError,OSError): pass
    print('PASS: real workspace/terminal creation at reported Mac path length, socket/control attachment, existing PID/state/themes retained, legacy IDs retained, overlong paths fail before mutation')


if __name__=='__main__': main()
