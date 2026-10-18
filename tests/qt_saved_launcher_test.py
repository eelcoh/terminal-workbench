"""Saved Qt launches preserve real shells/catalogs across normal and failed exits."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tests')]
from session_service import request
from session_smoke import wait_until
from window_slots import acquire_window_slot
runtime=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='wb-qt-saved-',dir='/tmp') as directory:
    root=Path(directory);state=root/'sessions';viewer=root/'viewer';evidence=root/'evidence.json'
    viewer.write_text('#!'+sys.executable+'\n'+
        'import sys\nsys.path[:0]='+repr([str(ROOT/'scripts'),str(ROOT/'tests')])+'\n'+r"""import json,os,shlex
from pathlib import Path
from session_service import request
from session_smoke import attach,send,wait_until
path=Path(sys.argv[1]);config=json.loads(path.read_text());state=Path(config['state']);runtime=Path(config['runtime']);cwd=Path(config['cwd'])
data=config['catalog'];terminal=data['workspaces'][0]['terminals'][0]['id'];pid=None
if not os.environ.get('WB_SAVED_STOPPED'):
 connection,_,_=attach(state/(terminal+'.sock'))
 try:
  proof=state/'reader-proof';proof.unlink(missing_ok=True)
  send(connection,16,('printf "%s" "$$" > '+shlex.quote(str(proof))+'\n').encode())
  wait_until(lambda:proof.exists() and proof.read_text().strip());pid=int(proof.read_text())
 finally:connection.close()
if os.environ.get('WB_SAVED_CREATE'):
 data=request(state,runtime,cwd,'workspace.create_at',title='Retained project',project_directory=str(cwd))['data']
Path(os.environ['WB_SAVED_EVIDENCE']).write_text(json.dumps(dict(config_path=str(path),catalog=data,pid=pid,terminal=terminal,view=config['view_state'],saved=config['saved_session'],state=config['state'],cwd=config['cwd'])))
sys.exit(int(os.environ.get('WB_SAVED_EXIT','0')))
""");viewer.chmod(0o700)
    env=dict(os.environ,SHELL='/bin/sh',WORKBENCH_STATE_DIR=str(state),WORKBENCH_CONFIG_DIR=str(root/'config'),WB_SAVED_EVIDENCE=str(evidence))
    def launch(**extra):
        result=subprocess.run([sys.executable,str(ROOT/'scripts/toolkit-ui.py'),str(runtime),str(viewer),'--saved'],env=dict(env,**extra),capture_output=True,text=True,timeout=90)
        assert result.returncode==int(extra.get('WB_SAVED_EXIT','0')),(result.stdout,result.stderr)
        data=json.loads(evidence.read_text());assert data['saved'] and Path(data['state'])==state and Path(data['cwd'])==ROOT
        assert not Path(data['config_path']).exists(),'temporary viewer config retained'
        assert 'Closing detaches' in result.stdout
        return data
    try:
        first=launch(WB_SAVED_CREATE='1');os.kill(first['pid'],0)
        service_pid=request(state,runtime,ROOT,'health')['service_pid']
        assert Path(first['view']).name=='1.json'
        lease,_=acquire_window_slot(state/'qt')
        try:
            peer=launch();assert Path(peer['view']).name=='2.json' and peer['pid']==first['pid'],'window lease isolation'
        finally:os.close(lease)
        second=launch();assert second['view']==first['view'];assert second['catalog']==first['catalog'] and second['pid']==first['pid'],'relaunch replaced a shell or workspace'
        failed=launch(WB_SAVED_EXIT='42');assert failed['pid']==first['pid'] and failed['catalog']==first['catalog'];os.kill(first['pid'],0)
        assert request(state,runtime,ROOT,'health')['service_pid']==service_pid,'viewer exit stopped the service'
        request(state,runtime,ROOT,'session.stop',terminal_ids=[first['terminal']])
        def stopped():
            return next(t['state'] for t in request(state,runtime,ROOT,'health')['terminals'] if t['id']==first['terminal'])=='stopped'
        wait_until(stopped)
        last=launch(WB_SAVED_STOPPED='1');assert last['catalog']==first['catalog'] and stopped(),'reopening restarted a stopped saved terminal'
    finally:
        if (state/'workspaces.json').exists():
            data=request(state,runtime,ROOT)['data'];ids=[t['id'] for w in data['workspaces'] for t in w['terminals']]
            if ids:request(state,runtime,ROOT,'session.stop',terminal_ids=ids)
        request(state,runtime,ROOT,'service.shutdown')
print('PASS: saved Qt launcher retains catalog, service and same-PID shells across normal/failed exits; stopped entries do not restart')
