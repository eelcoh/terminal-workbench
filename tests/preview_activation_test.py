"""Switch staged services without changing shell identity or saved user state."""
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tests')]
from session_service import request,exchange,PROTOCOL,Unavailable
from session_smoke import attach,send,wait_until
from window_slots import acquire_window_slot,reserve_window_slots
spec=importlib.util.spec_from_file_location('stage_toolkit',ROOT/'scripts/stage-toolkit.py')
staging=importlib.util.module_from_spec(spec);spec.loader.exec_module(staging)
runtime=Path(sys.argv[1]).resolve()
built=ROOT/'.local/build/qt-input/workbench-ui-study'
if sys.platform=='darwin':built=built.with_suffix('.app')/'Contents/MacOS/workbench-ui-study'

with tempfile.TemporaryDirectory(prefix='wb-update-',dir='/tmp') as directory:
    root=Path(directory).resolve();home=root/'home';home.mkdir();state=root/'sessions';config=root/'preferences'
    env={k:v for k,v in os.environ.items() if not k.startswith(('WORKBENCH_','XDG_'))}
    env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',HOME=str(home),SHELL='/bin/sh',WORKBENCH_STATE_DIR=str(state),WORKBENCH_CONFIG_DIR=str(config),PYTHONDONTWRITEBYTECODE='1')
    stages=[staging.stage(root/f'build-{i}',runtime,built,ROOT/'.local/build/qt-input/workbench-preview-bootstrap',sign=sys.platform=='darwin') for i in range(3)]
    def payload(index):
        return stages[index]/'Terminal Workbench Preview.app/Contents/Resources/workbench' if sys.platform=='darwin' else stages[index]
    binaries=[payload(i)/'libexec/workbench-session' for i in range(3)]
    def cli(index,action,code=0,**environment):
        result=subprocess.run([str(stages[index]/'terminal-workbench'),action],env=dict(env,**environment),capture_output=True,text=True,timeout=45)
        assert result.returncode==code,(result.stdout,result.stderr)
        return result
    def health():return exchange(state,dict(protocol=PROTOCOL,method='health'))
    before=json.loads(cli(0,'--status').stdout)
    assert before['service'] is None and not state.exists() and not config.exists(),'status started or created state'
    try:request(state,binaries[0],home,'service.retire',expected_instance='stale')
    except Unavailable:pass
    else:raise AssertionError('retirement of absent service started an authority')
    assert not state.exists()
    candidate=payload(1)/'../../MacOS/workbench-ui-study' if sys.platform=='darwin' else payload(1)/'libexec/workbench-ui-study'
    original_viewer=candidate.read_bytes()
    try:
        candidate.write_text('#!/bin/sh\nexit 42\n')
        result=cli(1,'--activate',1)
        assert 'activation checks failed' in result.stderr and '--check' in result.stderr
        assert not state.exists() and not config.exists(),'failed candidate created user state'
    finally:candidate.write_bytes(original_viewer)
    initial=json.loads(cli(0,'--activate').stdout.split('\nPreview service selected.',1)[0])
    assert initial['verification']['ok']
    assert not (state/'workspaces.json').exists(),'activation seeded terminals before a window opened'
    assert health()['terminals']==[]
    connection=None
    try:
        data=request(state,binaries[0],home)['data']
        workspace=data['workspaces'][0]['id'];terminal=data['workspaces'][0]['terminals'][0]['id'];stopped=data['workspaces'][0]['terminals'][1]['id']
        request(state,binaries[0],home,'workspace.rename',workspace,'Retained project')
        request(state,binaries[0],home,'session.stop',terminal_ids=[stopped])
        wait_until(lambda:next(t['state'] for t in health()['terminals'] if t['id']==stopped)=='stopped')
        connection,_,_=attach(state/(terminal+'.sock'))
        proof=home/'before';send(connection,16,('retained=kept; printf "%s:%s:%s" "$$" "$retained" "$WORKBENCH_AGENT_BIN" > '+shlex.quote(str(proof))+'\n').encode())
        wait_until(lambda:proof.exists() and proof.read_text().count(':')==2)
        original_proof=proof.read_text();assert original_proof.endswith(str(payload(0)/'.local/bin'))
        config.mkdir();(config/'config.conf').write_text('managed preferences')
        view=state/'qt/windows/1.json';view.write_text('{"theme":"kept"}')
        saved=[state/'workspaces.json',state/'workspaces.tsv',view,config/'config.conf']
        snapshots={path:(path.read_bytes(),path.stat().st_mtime_ns) for path in saved}
        inodes={path:path.stat().st_ino for path in state.glob('*.sock')}
        first=health()
        usage=json.loads(cli(0,'--status').stdout)['payload_usage']
        assert usage['service_available'] and not usage['unknown_terminals']
        assert len(usage['builds'])==1
        assert terminal in usage['builds'][0]['runtime_terminals'] and stopped not in usage['builds'][0]['runtime_terminals']
        assert usage['builds'][0]['helper_terminals']==usage['builds'][0]['runtime_terminals']
        # A broken candidate is rejected before retiring the healthy service.
        try:
            candidate.write_text('#!/bin/sh\nexit 42\n')
            result=cli(1,'--activate',1)
            assert 'qml_startup' in result.stderr and 'current service was left unchanged' in result.stderr
            unchanged=health()
            assert unchanged['service_instance']==first['service_instance'] and unchanged['service_pid']==first['service_pid']
            for path,(contents,stamp) in snapshots.items():assert path.read_bytes()==contents and path.stat().st_mtime_ns==stamp,path
            assert all(path.stat().st_ino==inode for path,inode in inodes.items())
        finally:candidate.write_bytes(original_viewer)
        # A currently open preview blocks activation before the service is touched.
        lease,_=acquire_window_slot(state/'qt')
        try:
            result=cli(1,'--activate',1);assert 'Close all preview windows' in result.stderr
            assert health()['service_instance']==first['service_instance']
        finally:os.close(lease)
        # Reservation uses the same finite slots old launchers understand. A
        # blocked launch cannot initialize the mismatched service or attach PTYs.
        with reserve_window_slots(state/'qt'):
            result=subprocess.run([str(stages[1]/'terminal-workbench')],env=env,capture_output=True,text=True,timeout=15)
            assert result.returncode!=0 and 'All 64 window slots' in result.stderr,(result.stdout,result.stderr)
        changed=json.loads(cli(1,'--status').stdout);assert changed['activation_required']
        cli(1,'--activate');second=health()
        assert second['service_instance']!=first['service_instance'] and second['service_pid']!=first['service_pid']
        assert second['runtime_binary']==str(binaries[1])
        selected_status=json.loads(cli(1,'--status').stdout)
        assert not selected_status['activation_required']
        usage=selected_status['payload_usage'];assert not usage['unknown_terminals']
        old=next(build for build in usage['builds'] if build['directory']==str(payload(0)))
        chosen=next(build for build in usage['builds'] if build['directory']==str(payload(1)))
        assert terminal in old['runtime_terminals'] and terminal in old['helper_terminals']
        assert not old['selected'] and not old['service']
        assert chosen['selected'] and chosen['service'] and not chosen['runtime_terminals']
        assert next(t['state'] for t in second['terminals'] if t['id']==stopped)=='stopped'
        for path,(contents,stamp) in snapshots.items():assert path.read_bytes()==contents and path.stat().st_mtime_ns==stamp,path
        assert all(path.stat().st_ino==inode for path,inode in inodes.items())
        after=home/'after';send(connection,16,('printf "%s:%s:%s" "$$" "$retained" "$WORKBENCH_AGENT_BIN" > '+shlex.quote(str(after))+'\n').encode())
        wait_until(lambda:after.exists() and after.read_text()==original_proof)
        # A stale retirement request cannot shut down the new service.
        for instance in (None,'',first['service_instance']):
            try:exchange(state,dict(protocol=PROTOCOL,method='service.retire',expected_instance=instance))
            except ValueError as error:assert 'changed' in str(error)
            else:raise AssertionError('stale retirement stopped a replacement service')
        assert health()['service_instance']==second['service_instance']
        cli(1,'--activate');assert health()['service_instance']==second['service_instance'],'repeat activation restarted the selected service'
        # New shells use the selected service/runtime helpers; retained ones keep
        # their original helper paths, so old payload removal remains explicit.
        data=request(state,binaries[1],home,'terminal.create',workspace,'New shell')['data']
        fresh=data['workspaces'][0]['terminals'][-1]['id'];reader,_,_=attach(state/(fresh+'.sock'))
        try:
            new=home/'new';send(reader,16,('printf "%s" "$WORKBENCH_AGENT_BIN" > '+shlex.quote(str(new))+'\n').encode())
            wait_until(lambda:new.exists() and new.read_text()==str(payload(1)/'.local/bin'))
        finally:reader.close()
        usage=json.loads(cli(1,'--status').stdout)['payload_usage']
        chosen=next(build for build in usage['builds'] if build['directory']==str(payload(1)))
        assert fresh in chosen['runtime_terminals'] and fresh in chosen['helper_terminals']
        assert not usage['unknown_terminals']
        # A failed new daemon leaves old shell processes and endpoint identities
        # intact. Retry with the known working build without recovering any shell.
        service=payload(2)/'scripts/session_service.py'
        source=service.read_text();source=source.replace('if __name__ == "__main__":','if __name__ == "__main__":\n    raise SystemExit(42)')
        service.write_text(source)
        result=cli(2,'--activate',1);assert 'did not start' in result.stderr
        assert json.loads(cli(1,'--status').stdout)['service'] is None
        assert all(path.stat().st_ino==inode for path,inode in inodes.items())
        cli(1,'--activate')
        assert next(t['state'] for t in health()['terminals'] if t['id']==stopped)=='stopped'
        resumed=home/'resumed';send(connection,16,('printf "%s:%s:%s" "$$" "$retained" "$WORKBENCH_AGENT_BIN" > '+shlex.quote(str(resumed))+'\n').encode())
        wait_until(lambda:resumed.exists() and resumed.read_text()==original_proof)
        lease,slot=acquire_window_slot(state/'qt');os.close(lease);assert slot.name=='1.view','activation leaked a window reservation'
        # A non-preview namespace must not be retired by preview activation.
        request(state,binaries[1],home,'service.shutdown')
        unrelated=root/'unrelated';unrelated.mkdir()
        request(state,runtime,unrelated,'health');other=health()
        result=cli(1,'--activate',1);assert 'different initial directory' in result.stderr
        assert health()['service_instance']==other['service_instance']
        request(state,runtime,unrelated,'service.shutdown')
        cli(1,'--activate')
    finally:
        if connection:connection.close()
        current=request(state,binaries[1],home,'health')
        data=exchange(state,dict(protocol=PROTOCOL,method='catalog.read'))['data']
        ids=[t['id'] for w in data['workspaces'] for t in w['terminals']]
        if ids:request(state,binaries[1],home,'session.stop',terminal_ids=ids)
        usage=json.loads(cli(1,'--status').stdout)['payload_usage']
        assert all(not row['runtime_terminals'] and not row['helper_terminals'] for row in usage['builds'])
        assert not usage['unknown_terminals']
        request(state,binaries[1],home,'service.shutdown')
    # Legacy services are observed but never sent an unsafe unconditional stop.
    legacy=root/'legacy';legacy.mkdir();observed=root/'legacy-request'
    server=root/'legacy.py';server.write_text('''import json,socket,sys
from pathlib import Path
root=Path(sys.argv[1]);s=socket.socket(socket.AF_UNIX);s.bind(str(root/'service.rpc'));s.listen()
c,_=s.accept();data=b''
while b'\\n' not in data:data+=c.recv(4096)
Path(sys.argv[2]).write_bytes(data)
c.sendall((json.dumps(dict(protocol=1,ok=True,runtime_binary=sys.argv[3],directory=sys.argv[4],methods=['health','service.shutdown']))+'\\n').encode());c.close();s.close()
''')
    process=subprocess.Popen([sys.executable,str(server),str(legacy),str(observed),str(binaries[0]),str(home)])
    try:
        wait_until((legacy/'service.rpc').exists)
        result=cli(1,'--activate',1,WORKBENCH_STATE_DIR=str(legacy));assert 'older preview service' in result.stderr
        process.wait(timeout=5);assert json.loads(observed.read_text())['method']=='health'
    finally:
        if process.poll() is None:process.kill();process.wait()
print('PASS: candidate startup-check rejection before state creation/retirement, explicit preview handoff, stale-instance refusal, active-window exclusion, unchanged catalog/preferences/views and shell PIDs/variables/sockets; retained/new preview-origin reporting, stopped-shell retention, startup-failure retry, legacy and unrelated-service isolation')
