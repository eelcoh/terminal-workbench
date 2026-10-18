"""Launch a private session service for the Qt UI study; never saved user state."""
import json
import os
from config_paths import config_directory
from pathlib import Path
import subprocess
import sys
import tempfile
from session_service import request
from workspace_catalog import load

runtime,viewer=[Path(value).resolve() for value in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='wb-ui-',dir='/tmp') as directory:
    state=Path(directory);cwd=Path.home().resolve()
    def call(method='initialize',identity='',title=''):
        return request(state,runtime,cwd,method,identity,title)['data']
    try:
        data=call()
        for workspace,title in zip(list(data['workspaces']),('Workbench','Side project')):
            call('workspace.rename',workspace['id'],title)
            data=call('terminal.create',workspace['id'],'Shell')
        data=call('workspace.create',title='Empty workspace');empty=data['workspaces'][-1]
        data=call('terminal.close',empty['terminals'][0]['id'])
        config=state/'ui.json';config.write_text(json.dumps({'state':str(state),'runtime':str(runtime),'cwd':str(cwd),'catalog':data,'preferences':str(config_directory(os.environ,sys.platform)/'config.conf'),'theme_store':str(config_directory(os.environ,sys.platform)/'themes'/'imports')}))
        print('UI study: private disposable sessions. Closing stops every shell created in this study.',flush=True)
        subprocess.run([str(viewer),str(config)],check=True)
    finally:
        # Include shells created after launch and sockets from a partial creation.
        try:
            if (state/'workspaces.json').exists():
                ids=[t['id'] for w in load(state)['workspaces'] for t in w['terminals']]
                if ids:request(state,runtime,cwd,'session.stop',terminal_ids=ids)
        finally:
            for sock in state.glob('*.sock'):
                subprocess.run([str(runtime),'stop',str(sock)],capture_output=True,timeout=12)
            request(state,runtime,cwd,'service.shutdown')
