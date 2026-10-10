#!/usr/bin/env python3
"""Reboot recovery preserves live sessions, identities and non-socket files."""
from pathlib import Path
import copy
import importlib.util
import os
import socket
import subprocess
import sys
import tempfile
from session_smoke import attach, send, wait_until

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('catalog',ROOT/'scripts/workspace_catalog.py')
catalog=importlib.util.module_from_spec(spec); spec.loader.exec_module(catalog)


def dead_socket(path):
    with socket.socket(socket.AF_UNIX) as connection: connection.bind(str(path))


def main():
    runtime=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='wb-recover-') as directory:
        state=Path(directory)
        data={'version':1,'workspaces':[{'id':'w0','name':'Projects','terminals':[
            {'id':'live','name':'Live shell'},{'id':'dead','name':'After reboot'}]}]}
        catalog.save(state,data)
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
                try: catalog.manage(state,runtime,state)
                except ValueError as error: assert 'just recover-sessions' in str(error)
                else: raise AssertionError('stale socket silently reused')
                assert (state/'workspaces.json').read_bytes()==saved and (state/'dead.sock').exists()
                recovered=catalog.manage(state,runtime,state,'session.recover')
                assert recovered==data and preferences.read_bytes()==b'untouched settings'
                assert (state/'live.sock').stat().st_ino==inode and server.poll() is None
                connection,_,_=attach(state/'live.sock')
                send(connection,16,b'printf "%s" "$keep" > retained; echo $$ > live-after.pid\n')
                wait_until(lambda:(state/'retained').exists() and (state/'live-after.pid').exists())
                assert (state/'retained').read_text()=='retained' and (state/'live-after.pid').read_bytes()==pid
                connection.close()
                connection,_,_=attach(state/'dead.sock'); connection.close()
                # Either listening endpoint protects the pair, including startup transitions.
                extra=copy.deepcopy(data); extra['workspaces'][0]['terminals']=[{'id':'starting','name':'Starting'}]
                dead_socket(state/'starting.sock')
                with socket.socket(socket.AF_UNIX) as control:
                    control.bind(str(state/'starting.ctl')); control.listen()
                    catalog.recover_sessions(state,extra)
                    assert (state/'starting.sock').exists()
                # Reject regular files and symlinks without removing another dead endpoint.
                extra['workspaces'][0]['terminals']=[{'id':'safe','name':'Dead'},{'id':'unsafe','name':'File'}]
                dead_socket(state/'safe.sock')
                (state/'unsafe.sock').write_bytes(b'keep me')
                for symlink in (False,True):
                    if symlink:
                        (state/'unsafe.sock').unlink(); (state/'unsafe.sock').symlink_to(preferences)
                    try: catalog.recover_sessions(state,extra)
                    except ValueError: pass
                    else: raise AssertionError('non-socket recovered')
                    assert (state/'safe.sock').exists() and preferences.read_bytes()==b'untouched settings'
                print('PASS: refused startup diagnosed, dead endpoints recovered, live PID/shell state retained, stable catalog/themes, control-only listener protected, regular files/symlinks preserved')
            finally:
                for identity in ('live','dead'):
                    subprocess.run([str(runtime),'stop',str(state/f'{identity}.sock')],capture_output=True,timeout=15)
                if server.poll() is None: server.terminate()
                server.wait(timeout=5)


if __name__=='__main__': main()
