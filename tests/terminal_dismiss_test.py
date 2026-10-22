"""Stopped-terminal removal cannot stop/reconcile/recover a live runtime."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json,os,socket,subprocess,sys,tempfile
from unittest.mock import patch
ROOT=Path(__file__).resolve().parent.parent
sys.path[:0]=[str(ROOT/'scripts'),str(ROOT/'tests')]
import workspace_catalog as catalog
from session_service import request,Authority
from session_smoke import attach,send,wait_until,expect
runtime=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='wb-dismiss-',dir='/tmp') as temporary:
    root=Path(temporary);state=root/'sessions';os.environ['SHELL']='/bin/sh';viewers=[]
    def call(method='initialize',identity=''):return request(state,runtime,root,method,identity)
    def terminal_state(identity):return call('terminal.status',identity)['state']
    def reject(identity):
        before=(state/'workspaces.json').read_bytes()
        try:call('terminal.dismiss',identity)
        except ValueError:pass
        else:raise AssertionError('dismissed a live/missing identity')
        assert (state/'workspaces.json').read_bytes()==before
    try:
        data=call()['data'];ids=[t['id'] for w in data['workspaces'] for t in w['terminals']];target,sibling=ids[:2]
        first,_,_=attach(catalog.socket_path(state,{'id':target}));second,_,_=attach(catalog.socket_path(state,{'id':sibling}));viewers=[first,second]
        send(first,16,b'keep=dismiss-survivor; printf WB_DISMISS_READY\n');expect(first,b'WB_DISMISS_READY')
        reject(target);reject('missing')
        send(first,16,b'printf "%s" "$keep"\n');expect(first,b'dismiss-survivor')
        send(second,16,b'exit 42\n');wait_until(lambda:terminal_state(sibling)=='stopped')
        evidence=catalog.socket_path(state,{'id':sibling}).with_suffix('.lifecycle');record=evidence.read_bytes()
        result=call('terminal.dismiss',sibling)['data']
        assert [w['id'] for w in result['workspaces']]==[w['id'] for w in data['workspaces']]
        assert all(t['id']!=sibling for w in result['workspaces'] for t in w['terminals']) and evidence.read_bytes()==record
        assert terminal_state(target)=='running';send(first,16,b'printf "%s" "$keep"\n');expect(first,b'dismiss-survivor')
        # Recovery and dismissal serialize on the same catalog lock. Either the
        # recovered shell survives, or removal wins and recovery cannot retarget.
        racer=next(t['id'] for w in result['workspaces'] for t in w['terminals'] if t['id']!=target)
        stream,_,_=attach(catalog.socket_path(state,{'id':racer}));viewers.append(stream);send(stream,16,b'exit\n');wait_until(lambda:terminal_state(racer)=='stopped')
        def race(method):
            try:return method,call(method,racer),None
            except ValueError as error:return method,None,str(error)
        with ThreadPoolExecutor(max_workers=2) as pool:outcomes=list(pool.map(race,['terminal.recover','terminal.dismiss']))
        assert sum(error is None for _,_,error in outcomes)==1
        winner=next(method for method,_,error in outcomes if error is None)
        current=call('catalog.read')['data'];present=any(t['id']==racer for w in current['workspaces'] for t in w['terminals'])
        assert present==(winner=='terminal.recover')
        if present:assert terminal_state(racer)=='running'
    finally:
        for stream in viewers:stream.close()
        if state.exists():
            current=call('catalog.read')['data'];remaining=[t['id'] for w in current['workspaces'] for t in w['terminals']]
            if remaining:request(state,runtime,root,'session.stop',terminal_ids=remaining)
            call('service.shutdown')
# Direct authority cases assert no runtime command, journal reconciliation or
# endpoint cleanup occurs, including uncertain/unsafe path evidence.
with tempfile.TemporaryDirectory(prefix='wb-dismiss-evidence-',dir='/tmp') as temporary:
    root=Path(temporary);data={'version':1,'workspaces':[{'id':'w','name':'Workspace','terminals':[{'id':'t','name':'Exited'}]}]};catalog.save(root,data)
    authority=Authority(root,runtime,root);message=dict(protocol=1,method='terminal.dismiss',identity='t',runtime=str(runtime),cwd=str(root))
    endpoint=root/'t.ctl';endpoint.write_text('preserved')
    for unsafe in ('regular','symlink','unavailable','journal'):
        if unsafe=='symlink':endpoint.unlink();endpoint.symlink_to(root/'missing')
        if unsafe=='unavailable':endpoint.unlink()
        if unsafe=='journal':catalog.save(root,dict(data,pending_create=['t']))
        before=(root/'workspaces.json').read_bytes()
        with patch.object(catalog.subprocess,'run',side_effect=AssertionError('dismiss ran a runtime command')),patch.object(catalog,'finish_close',side_effect=AssertionError('dismiss reconciled a close')):
            try:
                if unsafe=='unavailable':
                    with patch.object(catalog,'endpoint_alive',side_effect=ValueError('uncertain endpoint')):authority.dispatch(message)
                else:authority.dispatch(message)
            except ValueError:pass
            else:raise AssertionError('uncertain/pending terminal was dismissed')
        assert (root/'workspaces.json').read_bytes()==before
    catalog.save(root,data)
    with patch.object(catalog.subprocess,'run',side_effect=AssertionError('dismiss ran a runtime command')):
        value=authority.dispatch(message)
    assert value['data']['workspaces'][0]['terminals']==[]
    (root/'workspaces.json').unlink()
    with patch.object(catalog,'seed',side_effect=AssertionError('dismiss seeded a catalog')):
        try:authority.dispatch(message)
        except ValueError:pass
        else:raise AssertionError('dismiss created an absent catalog')
    assert not (root/'workspaces.json').exists()

print('PASS: immediate stopped-only removal, retained sibling input/evidence/workspaces, recovery race, unsafe/uncertain endpoint and pending-journal isolation; no runtime commands')
