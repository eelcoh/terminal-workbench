"""Copilot launcher/plugin contracts and silent hook failures; state mapping is
covered by the runtime's agent_hooks unit tests."""
from pathlib import Path
import json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parent.parent
EVENTS={'sessionStart','sessionEnd','userPromptSubmitted','preToolUse','postToolUse',
    'postToolUseFailure','agentStop','errorOccurred','notification'}

def main():
    runtime=str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='wb-copilot-launch-') as directory:
        temp=Path(directory);fake=temp/'copilot'
        fake.write_text('#!/usr/bin/env python3\nimport json,os,sys\nprint(json.dumps({"args":sys.argv[1:],"group":os.environ["WORKBENCH_AGENT_GROUP"],"run":os.environ["WORKBENCH_AGENT_RUN"],"policy":os.environ.get("COPILOT_HOME")}))\n');fake.chmod(0o700)
        policy=temp/'existing-policy';policy.mkdir()
        settings=policy/'settings.json';original=json.dumps({'disableAllHooks':True})
        settings.write_text(original)
        subprocess.run([sys.executable,str(ROOT/'scripts/prepare-agent-launchers.py')],check=True)
        bin=ROOT/'.local/bin'
        env=dict(os.environ,PATH=str(temp)+os.pathsep+os.environ['PATH'],WORKBENCH_SESSION_SOCKET='/tmp/fixture.sock',
                 COPILOT_HOME=str(policy),WORKBENCH_AGENT_BIN=str(bin))
        for args in ([runtime,'agent','copilot','--resume','a spaced name'],[str(bin/'wb-copilot'),'--resume','a spaced name']):
            result=subprocess.run(args,env=env,capture_output=True,text=True,check=True)
            capture=json.loads(result.stdout)
            assert capture['args'][:1]==['--plugin-dir'] and capture['args'][2:]==['--resume','a spaced name']
            assert capture['group']==str(os.getpgrp()) and len(bytes.fromhex(capture['run']))==16
            assert capture['policy']==str(policy) and settings.read_text()==original
        plugin=Path(capture['args'][1]);manifest=json.loads((plugin/'plugin.json').read_text());hooks=json.loads((plugin/'hooks.json').read_text())
        assert manifest['hooks']=='hooks.json' and hooks['version']==1
        assert set(hooks['hooks'])==EVENTS
        for name,entries in hooks['hooks'].items():
            assert entries[0]['timeoutSec']==2 and entries[0]['bash']=='"$WORKBENCH_AGENT_BIN/wb-agent-hook" copilot '+name
        env.pop('WORKBENCH_SESSION_SOCKET')
        failed=subprocess.run(args,env=env,capture_output=True,text=True)
        assert failed.returncode!=0 and failed.stdout==''
        for raw in ('[]','{','x'*(1024*1024+1),json.dumps({'toolName':'ask_user'})):
            for hook in ([runtime,'agent-hook','copilot','preToolUse'],[str(bin/'wb-agent-hook'),'copilot','preToolUse']):
                silent=subprocess.run(hook,input=raw,env=env,capture_output=True,text=True,timeout=3)
                assert silent.returncode==0 and silent.stdout==silent.stderr==''
    print('PASS: Copilot launcher symlink and subcommand, policy preservation, invocation-local plugin/arguments and silent non-decisions')

if __name__=='__main__':main()
