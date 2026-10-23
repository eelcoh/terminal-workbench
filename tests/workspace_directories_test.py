#!/usr/bin/env python3
"""Real shell working directories, identity preservation and folder recovery."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile
from session_smoke import attach,send,wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request

def main():
    runtime=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='wb-project-') as temporary:
        root=Path(temporary);state=root/'sessions';os.environ['SHELL']='/bin/sh'
        first=root/'Projects/café files';first.mkdir(parents=True)
        second=root/'box $() project';second.mkdir()
        def call(method='initialize',identity='',title='',folder=None,destination=''):
            return request(state,runtime,root,method,identity,title,destination,project_directory=folder)
        def inspect(identity,marker):
            sock=state/f'{identity}.sock';connection,_,_=attach(sock)
            try:
                target=root/marker
                send(connection,16,('printf "%s\\n%s\\n%s" "$$" "$PWD" "$keep" > '+shlex.quote(str(target))+'\n').encode())
                return wait_until(lambda:target.exists() and target.read_text()).splitlines()
            finally:connection.close()
        def cleanup(directory):
            for sock in directory.glob('*.sock'):
                subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=15)
            request(directory,runtime,root,'service.shutdown')
        try:
            original=call()['data'];pid,cwd,*_=inspect('spike','original')
            assert cwd==str(root)
            inode=(state/'spike.sock').stat().st_ino
            appearance=state/'appearance.state';appearance.write_bytes(b'keep colors')
            updated=call('workspace.directory','workspace-0',folder='Projects/café files')['data']
            assert updated['workspaces'][0]['directory']==str(first)
            assert inspect('spike','after-change')[:2]==[pid,str(root)]
            assert (state/'spike.sock').stat().st_ino==inode and appearance.read_bytes()==b'keep colors'
            created=call('terminal.create','workspace-0','New terminal')['data']['workspaces'][0]['terminals'][-1]['id']
            newpid,newcwd,*_=inspect(created,'new-terminal');assert newcwd==str(first)
            data=call('workspace.create_at',title='Remote box',folder=str(second))['data']
            workspace=data['workspaces'][-1];other=workspace['terminals'][0]['id']
            assert inspect(other,'first-shell')[1]==str(second)
            assert (state/'workspaces.tsv').read_text().startswith('workbench-catalog-v4\n')
            before=(state/'workspaces.json').read_bytes()
            file=root/'file';file.write_text('not a folder')
            for folder in (str(root/'missing'),str(file),'bad\npath','x'*1024,None,[],123):
                for method in ('workspace.directory','workspace.create_at'):
                    try:call(method,'workspace-0','Invalid',folder)
                    except (ValueError,TypeError):pass
                    else:raise AssertionError(('invalid folder accepted',folder))
                    assert (state/'workspaces.json').read_bytes()==before
            # Moving a live terminal changes only future recovery's workspace folder.
            call('terminal.move',created,destination=workspace['id'])
            assert inspect(created,'moved')[:2]==[newpid,str(first)]
            subprocess.run([str(runtime),'stop',str(state/f'{created}.sock')],check=True,timeout=15)
            call('terminal.recover',created)
            recovered=inspect(created,'recovered');assert recovered[0]!=newpid and recovered[1]==str(second)
            # A removed saved folder neither breaks adoption nor silently falls back.
            second.rmdir();before=(state/'workspaces.json').read_bytes()
            assert call()['data']['workspaces'][-1]['directory']==str(second)
            assert inspect(other,'adopted-missing')[1]==str(second)
            try:call('terminal.create',workspace['id'],'Missing folder')
            except ValueError as error:assert 'Folder' in str(error)
            else:raise AssertionError('missing folder fell back')
            assert (state/'workspaces.json').read_bytes()==before
            subprocess.run([str(runtime),'stop',str(state/f'{created}.sock')],check=True,timeout=15)
            try:call('terminal.recover',created)
            except ValueError as error:assert 'Folder' in str(error)
            else:raise AssertionError('recovery ignored missing folder')
            assert not (state/f'{created}.sock').exists()
            call('workspace.directory',workspace['id'],folder=str(root));call('terminal.recover',created)
            assert inspect(created,'fixed-folder')[1]==str(root)
        finally:cleanup(state)
    print('PASS: legacy defaults, workspace folder persistence, relative/Unicode/spaced paths, unchanged live shell PID/cwd, first/new/recovered shell cwd, moves, invalid/missing-folder isolation, adoption')
if __name__=='__main__':main()
