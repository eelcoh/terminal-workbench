"""Failed readiness reaps its own child; it never sends a queued mutation."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import session_service as service


class StartupCleanupTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-start-',dir='/tmp')
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        self.children=[]
        self.addCleanup(self.cleanup)

    def cleanup(self):
        for child in self.children:
            if child.poll() is None:service.stop_owned_startup(child)

    def spawn(self,source):
        script=self.root/f'child-{len(self.children)}.py';script.write_text(source)
        process=subprocess.Popen([sys.executable,str(script)],start_new_session=True,
            stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        self.children.append(process);return process

    def test_timeout_reaps_owned_child_and_preserves_unrelated_process(self):
        unrelated=self.spawn('import time\ntime.sleep(60)\n')
        child=self.spawn('import time\ntime.sleep(60)\n')
        with self.assertRaisesRegex(ValueError,'did not start'):
            service._wait_for_startup(self.root,dict(method='terminal.create'),child,timeout=.1)
        self.assertIsNotNone(child.returncode)
        self.assertIsNone(unrelated.poll(),'startup cleanup touched another process')

    def test_health_read_cannot_wait_past_startup_deadline(self):
        child=self.spawn('import socket,time\n'
            f's=socket.socket(socket.AF_UNIX);s.bind({str(self.root/"service.rpc")!r});s.listen()\n'
            'c,_=s.accept();time.sleep(60)\n')
        started=time.monotonic()
        with self.assertRaisesRegex(ValueError,'did not start'):
            service._wait_for_startup(self.root,dict(protocol=1,method='terminal.create'),child,timeout=.2)
        self.assertIsNotNone(child.returncode)
        self.assertLess(time.monotonic()-started,3)

    def test_interrupt_during_readiness_reaps_child(self):
        child=self.spawn('import time\ntime.sleep(60)\n')
        with patch.object(service,'exchange',side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                service._wait_for_startup(self.root,{},child)
        self.assertIsNotNone(child.returncode)

    def test_lost_mutation_reply_after_startup_keeps_service_and_never_replays(self):
        script=self.root/'authority.py';observed=self.root/'observed'
        script.write_text('import json,socket,time\nfrom pathlib import Path\n'
            f's=socket.socket(socket.AF_UNIX);s.bind({str(self.root/"service.rpc")!r});s.listen()\n'
            'for i in range(2):\n c,_=s.accept();data=b""\n'
            ' while b"\\n" not in data:data+=c.recv(4096)\n'
            ' value=json.loads(data)\n'
            ' if i==0:\n  assert value["method"]=="health"\n  c.sendall((json.dumps(dict(protocol=1,ok=True))+chr(10)).encode())\n'
            f' else:Path({str(observed)!r}).write_text(json.dumps(value))\n'
            ' c.close()\ntime.sleep(60)\n')
        original=subprocess.Popen
        def start(*args,**kwargs):
            child=original(*args,**kwargs);self.children.append(child);return child
        with patch.object(service,'__file__',str(script)),patch.object(service.subprocess,'Popen',start):
            with self.assertRaises(service.ReplyClosed):
                service.request(self.root,self.root/'runtime',self.root,'terminal.create',title='once')
        self.assertEqual(json.loads(observed.read_text())['method'],'terminal.create')
        self.assertIsNone(self.children[-1].poll(),'lost mutation reply stopped a ready authority')

    def test_only_health_is_sent_and_ready_child_is_preserved(self):
        child=self.spawn('import time\ntime.sleep(60)\n')
        calls=[]
        def reply(directory,message,**kwargs):
            calls.append(message)
            if len(calls)==1:raise service.Unavailable()
            return dict(ok=True)
        with patch.object(service,'exchange',reply):
            service._wait_for_startup(self.root,dict(protocol=1,method='terminal.create',title='never replay'),child)
        self.assertEqual([item['method'] for item in calls],['health','health'])
        self.assertIsNone(child.poll(),'successful readiness stopped the authority')


if __name__=='__main__':unittest.main()
