#!/usr/bin/env python3
"""Older live organization services keep health fallback; local failures stay local."""
from pathlib import Path
from contextlib import redirect_stdout
from io import StringIO
from unittest.mock import patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'scripts'))
import workspace_catalog
import session_service

arguments=['catalog','--state-dir','/tmp/wb-status-fixture','--runtime','/tmp/runtime','--cwd','/tmp','--command','terminal.status','--id','one']
methods=[]
def older(directory,message):
    methods.append(message['method'])
    if message['method']=='terminal.status':raise ValueError('Unknown session service method')
    assert message['method']=='health'
    return {'terminals':[{'id':'one','state':'stopped'},{'id':'two','state':'running'}]}
with patch.object(sys,'argv',arguments),patch.object(session_service,'exchange',older),redirect_stdout(StringIO()) as output:
    assert workspace_catalog.main()==0
    assert output.getvalue()=='STATE\tone\tstopped\n'
    assert methods==['terminal.status','health']
with patch.object(sys,'argv',arguments),patch.object(session_service,'exchange',side_effect=ValueError('Incompatible protocol')),redirect_stdout(StringIO()) as output:
    assert workspace_catalog.main()==1 and 'Incompatible protocol' in output.getvalue()
with patch.object(sys,'argv',arguments),patch.object(session_service,'exchange',side_effect=session_service.Unavailable()),patch.object(session_service,'request',side_effect=AssertionError('read check restarted service')),redirect_stdout(StringIO()) as output:
    assert workspace_catalog.main()==1 and output.getvalue().startswith('ERROR\t')
print('PASS: old-service readonly health fallback, one-terminal identity, protocol rejection and unavailable-service non-restart')

methods.clear()
new_arguments=["terminal.lifecycle" if arg=="terminal.status" else arg for arg in arguments]
def oldest(directory,message):
    if message['method']=='terminal.lifecycle':
        methods.append(message['method']);raise ValueError('Unknown session service method')
    return older(directory,message)
with patch.object(sys,'argv',new_arguments),patch.object(session_service,'exchange',oldest),redirect_stdout(StringIO()) as output:
    assert workspace_catalog.main()==0
    assert output.getvalue()=='LIFECYCLE\tone\tstopped\tRuntime stopped; no exit reason was recorded.\n'
    assert methods==['terminal.lifecycle','terminal.status','health']
def previous(directory,message):
    if message['method']=='terminal.lifecycle':raise ValueError('Unknown session service method')
    assert message['method']=='terminal.status';return {'id':'one','state':'stopped'}
with patch.object(sys,'argv',new_arguments),patch.object(session_service,'exchange',previous),redirect_stdout(StringIO()) as output:
    assert workspace_catalog.main()==0 and output.getvalue().startswith('LIFECYCLE\tone\tstopped\t')
with patch.object(sys,'argv',new_arguments),patch.object(session_service,'exchange',side_effect=session_service.Unavailable()),patch.object(session_service,'request',side_effect=AssertionError('read check restarted service')),redirect_stdout(StringIO()) as output:
    assert workspace_catalog.main()==1 and output.getvalue().startswith('ERROR\t')
print('PASS: optional lifecycle method falls back through unchanged status/health without restarting an unavailable service')
