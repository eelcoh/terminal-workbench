#!/usr/bin/env python3
"""Real shell working directories, identity preservation and organization upgrades."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile
import time
from session_smoke import attach,send,wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request,endpoint,ReplyClosed
import importlib.util
spec=importlib.util.spec_from_file_location('launcher',ROOT/'scripts/run-spike.py')
launcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(launcher)

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
        # An old organization service can be replaced while retaining live shells.
        legacy_state=root/'upgrade';legacy_state.mkdir()
        wrapper="import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);import session_service as s;original=s.Authority.health;s.Authority.health=lambda self:{k:v for k,v in original(self).items() if k!='features'};s.serve(Path(sys.argv[2]),Path(sys.argv[3]),Path(sys.argv[4]))"
        legacy=subprocess.Popen([sys.executable,'-c',wrapper,str(ROOT/'scripts'),str(legacy_state),str(runtime),str(root)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            wait_until(lambda:endpoint(legacy_state).exists())
            request(legacy_state,runtime,root)
            sock=legacy_state/'spike.sock';connection,_,_=attach(sock)
            send(connection,16,('keep=upgrade; echo $$ > '+shlex.quote(str(root/'upgrade.pid'))+'\n').encode())
            pid=wait_until(lambda:(root/'upgrade.pid').exists() and (root/'upgrade.pid').read_text().strip());inode=sock.stat().st_ino
            # macOS can return EOF for a queued probe when the old listener
            # closes. Force that outcome deterministically on the first probe.
            original_exchange=launcher.exchange
            probes=[]
            def closing_probe(directory,message):
                probes.append(message['method'])
                if len(probes)==1:
                    raise ReplyClosed("Session service closed before replying")
                return original_exchange(directory,message)
            launcher.exchange=closing_probe
            try:
                service,health=launcher.ensure_service(legacy_state,runtime,root)
            finally:
                launcher.exchange=original_exchange
            assert len(probes)>=2 and set(probes)=={'health'}
            assert service['service_pid']!=legacy.pid and 'workspace.create_at' in health['methods']
            assert 'catalog-journal-v1' in health['features']
            legacy.wait(timeout=5)
            send(connection,16,('printf "%s:%s" "$$" "$keep" > '+shlex.quote(str(root/'upgraded'))+'\n').encode())
            assert wait_until(lambda:(root/'upgraded').exists() and (root/'upgraded').read_text())==pid+':upgrade'
            assert sock.stat().st_ino==inode;connection.close()
        finally:
            if legacy.poll() is None:legacy.terminate();legacy.wait(timeout=5)
            cleanup(legacy_state)
    print('PASS: legacy defaults, workspace folder persistence, relative/Unicode/spaced paths, unchanged live shell PID/cwd, first/new/recovered shell cwd, moves, invalid/missing-folder isolation, adoption and service upgrade with live state')
if __name__=='__main__':main()
