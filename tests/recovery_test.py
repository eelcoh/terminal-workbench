#!/usr/bin/env python3
"""Reboot recovery preserves live sessions, identities and non-socket files."""
from pathlib import Path
import os
import socket
import subprocess
import sys
import tempfile
from session_smoke import attach, send, wait_until

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import catalog_files
from session_service import request


def dead_socket(path):
    with socket.socket(socket.AF_UNIX) as connection: connection.bind(str(path))


def main():
    runtime=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='wb-recover-') as directory:
        state=Path(directory).resolve()
        def call(method='initialize',identity=''): return request(state,runtime,state,method,identity)
        data={'version':1,'workspaces':[{'id':'w0','name':'Projects','terminals':[
            {'id':'live','name':'Live shell'},{'id':'dead','name':'After reboot'}]}]}
        catalog_files.save(state,data)
        preferences=state/'appearance.state'; preferences.write_bytes(b'untouched settings')
        env=dict(os.environ,SHELL='/bin/sh')
        # Bootstrap one live runtime independently; simulate leftover dead endpoints.
        with (state/'fixture.log').open('wb') as log:
            server=subprocess.Popen([str(runtime),str(state/'live.sock'),'/bin/sh'],cwd=state,env=env,stdout=log,stderr=log)
            try:
                wait_until(lambda:(state/'live.sock').exists() and (state/'live.ctl').exists())
                connection,_,_=attach(state/'live.sock')
                send(connection,16,b'keep=retained; echo $$ > live.pid\n')
                wait_until(lambda:(state/'live.pid').exists())
                pid=(state/'live.pid').read_bytes(); inode=(state/'live.sock').stat().st_ino
                connection.close()
                dead_socket(state/'dead.sock'); dead_socket(state/'dead.ctl')
                saved=(state/'workspaces.json').read_bytes()
                # Saved startup leaves stale endpoints stopped and the catalog unchanged.
                assert call()['data']==data
                assert call('terminal.status','dead')['state']=='stopped'
                assert (state/'workspaces.json').read_bytes()==saved and (state/'dead.sock').exists()
                recovered=call('session.recover')['data']
                assert recovered==data and preferences.read_bytes()==b'untouched settings'
                assert (state/'live.sock').stat().st_ino==inode and server.poll() is None
                connection,_,_=attach(state/'live.sock')
                send(connection,16,b'printf "%s" "$keep" > retained; echo $$ > live-after.pid\n')
                wait_until(lambda:(state/'retained').exists() and (state/'live-after.pid').exists())
                assert (state/'retained').read_text()=='retained' and (state/'live-after.pid').read_bytes()==pid
                connection.close()
                connection,_,_=attach(state/'dead.sock'); connection.close()
                print('PASS: saved startup leaves stale endpoints stopped, explicit recovery restores dead endpoints, live PID/shell state retained, stable catalog/themes',flush=True)
            finally:
                for identity in ('live','dead'):
                    subprocess.run([str(runtime),'stop',str(state/f'{identity}.sock')],capture_output=True,timeout=15)
                if server.poll() is None: server.terminate()
                server.wait(timeout=5)
                call('service.shutdown')
    # Endpoint safety: separate catalogs so a refused plan cannot touch the cases above.
    with tempfile.TemporaryDirectory(prefix='wb-recover-guard-') as directory:
        state=Path(directory).resolve()
        def call(method='initialize'): return request(state,runtime,state,method)
        try:
            # Either listening endpoint protects the pair, including startup transitions.
            catalog_files.save(state,{'version':1,'workspaces':[{'id':'w','name':'W','terminals':[{'id':'starting','name':'Starting'}]}]})
            dead_socket(state/'starting.sock')
            with socket.socket(socket.AF_UNIX) as control:
                control.bind(str(state/'starting.ctl')); control.listen()
                call('session.recover')
                assert (state/'starting.sock').exists()
            (state/'starting.sock').unlink(); (state/'starting.ctl').unlink()
            # Regular files and symlinks are refused without removing another dead endpoint.
            catalog_files.save(state,{'version':1,'workspaces':[{'id':'w','name':'W','terminals':[
                {'id':'safe','name':'Dead'},{'id':'unsafe','name':'File'}]}]})
            preferences=state/'appearance.state'; preferences.write_bytes(b'untouched settings')
            dead_socket(state/'safe.sock')
            (state/'unsafe.sock').write_bytes(b'keep me')
            for symlink in (False,True):
                if symlink:
                    (state/'unsafe.sock').unlink(); (state/'unsafe.sock').symlink_to(preferences)
                try: call('session.recover')
                except ValueError: pass
                else: raise AssertionError('non-socket recovered')
                assert (state/'safe.sock').exists() and preferences.read_bytes()==b'untouched settings'
            print('PASS: control-only listener protected, regular files/symlinks preserved')
        finally:
            for path in state.glob('*.sock'):
                subprocess.run([str(runtime),'stop',str(path)],capture_output=True,timeout=15)
            call('service.shutdown')


if __name__=='__main__': main()
