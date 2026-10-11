"""Copilot observer schema, no decisions, additive launcher/plugin contracts."""
from pathlib import Path
import importlib.util,json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parent.parent
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
observer=module('observer',ROOT/'scripts/agent-hook.py')

def main():
    cases=[('sessionStart',{},3),('userPromptSubmitted',{},1),('preToolUse',{'toolName':'bash'},1),
        ('preToolUse',{'toolName':'ask_user'},2),('preToolUse',{'hook_event_name':'PreToolUse','tool_name':'AskUserQuestion'},2),
        ('notification',{'notification_type':'permission_prompt'},2),('notification',{'notification_type':'elicitation_dialog'},2),
        ('notification',{'notification_type':'agent_idle'},None),('notification',{'notification_type':'agent_completed'},None),
        ('postToolUse',{},1),('postToolUseFailure',{},1),('agentStop',{},3),('sessionEnd',{},0),('errorOccurred',{},0),
        ('permissionRequest',{},None),('subagentStop',{},None),('agentStop',{'hook_event_name':'SubagentStop'},None)]
    for name,event,expected in cases:
        assert observer.copilot_state(event,name)==expected,(name,event)
        for field in ('agent_id','agent_type','agentId','agentType','agentName','parentSessionId'):
            assert observer.copilot_state(dict(event,**{field:'child'}),name) is None
    for event in ([],None,'text'):assert observer.copilot_state(event,'agentStop') is None
    with tempfile.TemporaryDirectory(prefix='wb-copilot-launch-') as directory:
        temp=Path(directory);fake=temp/'copilot'
        fake.write_text('#!/usr/bin/env python3\nimport json,os,sys\nprint(json.dumps({"args":sys.argv[1:],"group":os.environ["WORKBENCH_AGENT_GROUP"],"run":os.environ["WORKBENCH_AGENT_RUN"],"policy":os.environ.get("COPILOT_HOME")}))\n');fake.chmod(0o700)
        policy=temp/'existing-policy';policy.mkdir()
        settings=policy/'settings.json';original=json.dumps({'disableAllHooks':True})
        settings.write_text(original)
        env=dict(os.environ,PATH=str(temp)+os.pathsep+os.environ['PATH'],WORKBENCH_SESSION_SOCKET='/tmp/fixture.sock',COPILOT_HOME=str(policy))
        subprocess.run(['python3',str(ROOT/'scripts/prepare-agent-launchers.py')],check=True)
        args=['python3',str(ROOT/'scripts/workbench-agent.py'),'copilot','--resume','a spaced name']
        result=subprocess.run(args,env=env,capture_output=True,text=True,check=True)
        capture=json.loads(result.stdout)
        assert capture['args'][:1]==['--plugin-dir'] and capture['args'][2:]==['--resume','a spaced name']
        assert capture['group']==str(os.getpgrp()) and len(bytes.fromhex(capture['run']))==16
        assert capture['policy']==str(policy) and settings.read_text()==original
        plugin=Path(capture['args'][1]);manifest=json.loads((plugin/'plugin.json').read_text());hooks=json.loads((plugin/'hooks.json').read_text())
        assert manifest['hooks']=='hooks.json' and hooks['version']==1
        assert 'permissionRequest' not in hooks['hooks'] and 'subagentStop' not in hooks['hooks']
        assert set(hooks['hooks'])==set(observer.COPILOT_EVENTS)
        for name,entries in hooks['hooks'].items():
            assert entries[0]['timeoutSec']==2 and name in entries[0]['bash']
        env.pop('WORKBENCH_SESSION_SOCKET')
        failed=subprocess.run(args,env=env,capture_output=True,text=True)
        assert failed.returncode!=0 and failed.stdout==''
        for raw in ('[]','{','x'*(1024*1024+1),json.dumps({'toolName':'ask_user'})):
            silent=subprocess.run(['python3',str(ROOT/'scripts/agent-hook.py'),'copilot','preToolUse'],input=raw,env=env,capture_output=True,text=True,timeout=3)
            assert silent.returncode==0 and silent.stdout==silent.stderr==''
    print('PASS: Copilot schemas, actual-prompt status, child/background rejection, silent non-decisions, policy preservation and invocation-local plugin/arguments')

if __name__=='__main__':main()
