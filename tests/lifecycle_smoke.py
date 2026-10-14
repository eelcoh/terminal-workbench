#!/usr/bin/env python3
"""Actual shell exits, runtime loss, service adoption and per-invocation evidence."""
from pathlib import Path
import json,os,signal,subprocess,sys,tempfile,time
from session_smoke import attach,send,wait_until
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from session_service import request
from terminal_lifecycle import detail,read_record
import workspace_catalog
runtime=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='wb-life-') as directory:
    root=Path(directory);state=root/'sessions';os.environ['SHELL']='/bin/sh';connection=None
    def call(method='initialize',identity=''):return request(state,runtime,root,method,identity)
    def receipt():return read_record(state/'spike.sock')
    def stopped():return call('terminal.lifecycle','spike')['state']=='stopped'
    def recover():
        call('terminal.recover','spike');wait_until(lambda:receipt() and receipt()['event']=='running')
        assert receipt()['exit_code'] is None and receipt()['signal'] is None
    try:
        data=call()['data'];sibling=read_record(state/'main-2.sock')['shell_pid']
        initial=receipt();assert initial['event']=='running' and initial['shell_pid']>1
        assert (state/'spike.lifecycle').stat().st_mode&0o777==0o600
        socket=state/'spike.sock';connection,_,_=attach(socket);send(connection,16,b'exit\n');wait_until(stopped);connection.close();connection=None
        result=call('terminal.lifecycle','spike');assert result['lifecycle']['reason']=='exited' and result['lifecycle']['record']['exit_code']==0
        assert {key: value for key,value in call('terminal.status','spike').items() if key not in ('ok','protocol')}=={'id':'spike','state':'stopped'},'Legacy status shape changed'
        old_service=call('health')['service_pid'];call('service.shutdown');time.sleep(.1)
        adopted=call('terminal.lifecycle','spike');assert adopted['lifecycle']['record']==receipt()
        assert call('health')['service_pid']!=old_service
        recover();assert receipt()['shell_pid']!=initial['shell_pid']
        connection,_,_=attach(socket);send(connection,16,b'exit 42\n');wait_until(stopped);connection.close();connection=None
        assert call('terminal.lifecycle','spike')['lifecycle']['record']['exit_code']==42
        recover();os.kill(receipt()['shell_pid'],signal.SIGKILL);wait_until(stopped)
        signalled=call('terminal.lifecycle','spike')['lifecycle'];assert signalled['reason']=='signal' and signalled['record']['signal'] and signalled['record']['exit_code'] is None
        recover();subprocess.run([str(runtime),'stop',str(socket)],check=True,capture_output=True,timeout=12);wait_until(stopped)
        assert call('terminal.lifecycle','spike')['lifecycle']['reason']=='stopped'
        recover();running=receipt();os.kill(running['runtime_pid'],signal.SIGKILL);wait_until(stopped)
        missing=call('terminal.lifecycle','spike')['lifecycle'];assert missing['reason']=='missing_exit' and missing['record']['event']=='running'
        recover();assert call('terminal.lifecycle','spike')['lifecycle']['reason']=='running'
        assert read_record(state/'main-2.sock')['shell_pid']==sibling and call('terminal.status','main-2')['state']=='running'
        # The new runtime resets evidence even when shell startup fails.
        failed=root/'failed.sock';result=subprocess.run([str(runtime),str(failed),'/no-such-shell'],capture_output=True,timeout=5)
        assert result.returncode!=0 and detail(failed,'stopped')['reason']=='startup_failed'
        # An older runtime launched through the catalog cannot inherit a prior exit.
        legacy=root/'legacy';legacy.mkdir();fake=root/'older-runtime'
        fake.write_text('#!'+sys.executable+'\nimport socket,sys,time\ns=socket.socket(socket.AF_UNIX)\ns.bind(sys.argv[1]);s.listen();time.sleep(30)\n');fake.chmod(0o700)
        path=legacy/'old.sock';record=dict(initial,event='exited',exit_code=42,signal=None)
        path.with_suffix('.lifecycle').write_text(json.dumps(record));path.with_suffix('.lifecycle').chmod(0o600)
        processes=[]
        try:
            workspace_catalog.ensure_sessions(legacy,{'workspaces':[{'id':'legacy','name':'Legacy','terminals':[{'id':'old','name':'Old'}]}]},fake,root,on_spawn=lambda identity,child:processes.append(child))
            processes[0].terminate();processes[0].wait(timeout=5)
            assert detail(path,'stopped')['reason']=='unknown' and read_record(path)['event']=='launching'
        finally:
            for child in processes:
                if child.poll() is None:child.terminate();child.wait(timeout=5)
        # Unsafe sidecars cannot redirect writes into unrelated files.
        guarded=root/'guarded.sock';target=root/'keep';target.write_text('untouched');guarded.with_suffix('.lifecycle').symlink_to(target)
        failed=subprocess.run([str(runtime),str(guarded),'/bin/sh'],capture_output=True,timeout=5)
        assert failed.returncode!=0 and target.read_text()=='untouched'
        print('PASS: exit 0/42, signal vs explicit stop, missing exit after runtime kill, service adoption, fresh recovery, legacy launch invalidation and safe evidence paths')
    finally:
        if connection:connection.close()
        if (state/'workspaces.json').exists():
            for w in json.loads((state/'workspaces.json').read_text())['workspaces']:
                for t in w['terminals']:subprocess.run([str(runtime),'stop',str(state/(t['id']+'.sock'))],capture_output=True,timeout=12)
        try:call('service.shutdown')
        except (OSError,ValueError):pass
