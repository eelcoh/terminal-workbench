"""UI launcher cleanup includes shells created after startup and failed viewer exits."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
ROOT=Path(__file__).resolve().parent.parent
runtime=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='wb-ui-launch-') as directory:
    root=Path(directory);viewer=root/'viewer'
    viewer.write_text('#!'+sys.executable+'\n'+'''import json,os,shlex,sys
from pathlib import Path
sys.path.insert(0,sys.argv[0]+'.scripts')
from session_service import request
from session_smoke import attach,send,wait_until
config=json.loads(Path(sys.argv[1]).read_text());state=Path(config['state']);runtime=Path(config['runtime']);cwd=Path(config['cwd'])
data=request(state,runtime,cwd,'terminal.create',config['catalog']['workspaces'][0]['id'],'Created later')['data']
data=request(state,runtime,cwd,'workspace.create_at',title='Created workspace',project_directory=str(cwd))['data']
pids=[]
for w in data['workspaces']:
 for t in w['terminals']:
  path=state/(t['id']+'.sock');c,_,_=attach(path);target=state/(t['id']+'.pid')
  send(c,16,('echo $$ > '+shlex.quote(str(target))+'\\n').encode());wait_until(lambda:target.exists() and target.read_text().strip());pids.append(int(target.read_text()));c.close()
pids.append(request(state,runtime,cwd,'health')['service_pid'])
Path(os.environ['WB_UI_EVIDENCE']).write_text(json.dumps({'pids':pids,'state':str(state),'count':sum(len(w['terminals']) for w in data['workspaces'])}))
sys.exit(int(os.environ['WB_UI_EXIT']))
''');viewer.chmod(0o700)
    # A private import directory supplies existing test helpers to this fake viewer.
    imports=Path(str(viewer)+'.scripts');imports.mkdir()
    for name,path in [('session_service.py',ROOT/'scripts/session_service.py'),('workspace_catalog.py',ROOT/'scripts/workspace_catalog.py'),('session_smoke.py',ROOT/'tests/session_smoke.py')]:
        (imports/name).symlink_to(path)
    for exit_code in (0,42):
        evidence=root/'evidence.json';env=dict(os.environ,WORKBENCH_CONFIG_DIR=str(root/"config"),SHELL='/bin/sh',WB_UI_EVIDENCE=str(evidence),WB_UI_EXIT=str(exit_code))
        result=subprocess.run([sys.executable,str(ROOT/'scripts/toolkit-ui.py'),str(runtime),str(viewer)],env=env,capture_output=True,text=True,timeout=90)
        assert (result.returncode==0)==(exit_code==0),(result.stdout,result.stderr)
        data=json.loads(evidence.read_text());assert data['count']==8
        assert not Path(data['state']).exists()
        deadline=time.monotonic()+5
        remaining=set(data['pids'])
        while remaining and time.monotonic()<deadline:
            for pid in list(remaining):
                try:os.kill(pid,0)
                except ProcessLookupError:remaining.remove(pid)
            if remaining:time.sleep(.02)
        assert not remaining,('study shell/service survived cleanup',remaining)
print('PASS: private UI launcher, post-launch terminal/workspace creation and complete shell/service cleanup on normal and failed viewer exit')
