"""App terminals run a command, stay in their workspace and are never restarted."""
from pathlib import Path
import json,os,re,sys,tempfile
ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tests')]
import catalog_files as catalog
from session_service import request
from session_smoke import attach,send,wait_until,expect
runtime=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='wb-apps-',dir='/tmp') as temporary:
    root=Path(temporary).resolve();state=root/'sessions';os.environ['SHELL']='/bin/sh';viewers=[]
    project=root/'project';(project/'sub').mkdir(parents=True)
    def call(method='initialize',identity='',title='',**extra):return request(state,runtime,root,method,identity,title,**extra)['data']
    def find(data,app_id):return next((t for w in data['workspaces'] for t in w['terminals'] if t.get('app',{}).get('id')==app_id),None)
    def state_of(identity):return request(state,runtime,root,'terminal.status',identity)['state']
    def refused(message,*args,**extra):
        before=(state/'workspaces.json').read_bytes()
        try:call(*args,**extra)
        except ValueError as error:assert message in str(error),error
        else:raise AssertionError('accepted: '+message)
        assert (state/'workspaces.json').read_bytes()==before
    try:
        call();workspace=call('workspace.create_at','','Project',project_directory=str(project))['workspaces'][-1]['id']
        other=call('workspace.create','','Other')['workspaces'][-1]['id']
        # The command runs through the login shell in the workspace folder,
        # or in the definition's folder, resolved when the app starts.
        app=dict(id='watch',command='printf "WB_APP %s\\n" "$PWD"; read line; printf "WB_GOT %s\\n" "$line"')
        data=call('terminal.create',workspace,'Watch',app=app);entry=find(data,'watch')
        assert entry['app']==app and entry['name']=='Watch' and state_of(entry['id'])=='running'
        stream,replayed,_=attach(catalog.socket_path(state,entry));viewers.append(stream)
        # The app may print before the viewer attaches; its output is then in the replay.
        printed=re.escape(('WB_APP '+str(project)).encode())
        if not re.search(printed,replayed):expect(stream,printed)
        located=call('terminal.create',workspace,'Sub',app=dict(id='sub',command='pwd; sleep 30',directory='sub'))
        assert find(located,'sub')['app']['directory']==str(project/'sub')
        refused('at most once','terminal.create',workspace,'Again',app=dict(id='watch',command='true'))
        refused('one line','terminal.create',workspace,'Bad',app=dict(id='bad',command='a\nb'))
        refused('Only terminal.create','workspace.rename',workspace,'Renamed',app=dict(id='x',command='true'))
        refused('stay in the workspace','terminal.move',entry['id'],destination=other)
        # The same app may run in another workspace.
        assert find(call('terminal.create',other,'Watch',app=dict(id='watch',command='sleep 30')),'watch')
        # When the program ends, the terminal ends with it, and the next window
        # removes it instead of keeping a stopped pane. A normal exit leaves no output.
        send(stream,16,b'done\n');expect(stream,b'WB_GOT done')
        wait_until(lambda:state_of(entry['id'])=='stopped')
        assert 'output' not in request(state,runtime,root,'terminal.lifecycle',entry['id'])['lifecycle']
        assert all(t['id']!=entry['id'] for w in call()['workspaces'] for t in w['terminals'])
        # An app that fails before any window attaches leaves its last lines
        # and exit code for the window that started it.
        failed=find(call('terminal.create',other,'Missing',app=dict(id='missing',command='echo WB_MISSING >&2; exit 3')),'missing')
        wait_until(lambda:state_of(failed['id'])=='stopped')
        detail=request(state,runtime,root,'terminal.lifecycle',failed['id'])['lifecycle']
        assert detail['record']['exit_code']==3 and detail['output'].strip()=='WB_MISSING',detail
        # Starting it again works once it has ended.
        again=find(call('terminal.create',workspace,'Watch',app=dict(id='watch',command='sleep 30')),'watch')
        assert again['id']!=entry['id'] and state_of(again['id'])=='running'
        # After a reboot every runtime is gone. Recovery restarts shells but
        # removes apps rather than restarting them.
        data=call('catalog.read');ids=[t['id'] for w in data['workspaces'] for t in w['terminals']]
        apps={t['id'] for w in data['workspaces'] for t in w['terminals'] if 'app' in t}
        request(state,runtime,root,'session.stop',terminal_ids=ids)
        wait_until(lambda:all(state_of(i)=='stopped' for i in ids))
        recovered=call('session.recover')
        left=[t for w in recovered['workspaces'] for t in w['terminals']]
        assert left and not any('app' in t for t in left) and {t['id'] for t in left}==set(ids)-apps
        assert all(state_of(t['id'])=='running' for t in left)
    finally:
        for stream in viewers:stream.close()
        if state.exists():
            current=request(state,runtime,root,'catalog.read')['data'];remaining=[t['id'] for w in current['workspaces'] for t in w['terminals']]
            if remaining:request(state,runtime,root,'session.stop',terminal_ids=remaining)
            request(state,runtime,root,'service.shutdown')
print('PASS: app command in its folder, uniqueness, validation, fixed workspace, removal on exit, output of a failed start, restart by definition, no restart on recovery')
