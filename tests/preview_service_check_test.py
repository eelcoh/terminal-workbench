"""Private candidate-service startup cannot leak processes or seed terminals."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import preview_service_check as probe


class ServiceCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-service-fixture-')
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        self.runtime=self.root/'libexec/workbench-session';self.runtime.parent.mkdir();self.runtime.touch()
        self.script=self.root/'scripts/session_service.py';self.script.parent.mkdir()
        self.children=[];original=probe.subprocess.Popen
        def spawn(*args,**kwargs):
            process=original(*args,**kwargs);self.children.append(process);return process
        self.spawn=patch.object(probe.subprocess,'Popen',spawn);self.spawn.start();self.addCleanup(self.spawn.stop)
        self.logs=[];original_log=probe.tempfile.mkstemp
        def log(*args,**kwargs):
            descriptor,path=original_log(*args,**kwargs);self.logs.append(Path(path));return descriptor,path
        self.logging=patch.object(probe.tempfile,'mkstemp',log);self.logging.start();self.addCleanup(self.logging.stop)
        self.addCleanup(lambda:[path.unlink(missing_ok=True) for path in self.logs])

    def assert_clean(self):
        self.assertTrue(self.children)
        for child in self.children:
            self.assertIsNotNone(child.poll(),'candidate service survived check')
            arguments=child.args
            state=Path(arguments[arguments.index('--state-dir')+1])
            self.assertFalse(state.exists(),'private namespace survived check')

    def test_early_failure_reports_private_log_and_reaps_child(self):
        self.script.write_text("print('fixture daemon failed',flush=True)\nraise SystemExit(42)\n")
        with self.assertRaisesRegex(ValueError,'exited 42') as error:probe.check(self.runtime,self.root)
        self.assert_clean()
        self.assertIn(str(self.logs[0]),str(error.exception))
        self.assertIn('fixture daemon failed',self.logs[0].read_text())

    def test_startup_timeout_cleans_up_without_sockets(self):
        self.script.write_text('import time\ntime.sleep(60)\n')
        with self.assertRaisesRegex(ValueError,'timed out'):probe.check(self.runtime,self.root,timeout=.15)
        self.assert_clean()

    def test_unresponsive_socket_is_bounded_and_reaped(self):
        self.script.write_text('import socket,sys,time\nfrom pathlib import Path\n'
            "state=Path(sys.argv[sys.argv.index('--state-dir')+1])\n"
            "s=socket.socket(socket.AF_UNIX);s.bind(str(state/'service.rpc'));s.listen()\n"
            'c,_=s.accept();time.sleep(60)\n')
        started=time.monotonic()
        with self.assertRaisesRegex(ValueError,'timed out'):probe.check(self.runtime,self.root,timeout=.3)
        self.assertLess(time.monotonic()-started,3)
        self.assert_clean()

    def test_incomplete_dripping_reply_cannot_extend_deadline(self):
        self.script.write_text('import socket,sys,time\nfrom pathlib import Path\n'
            "state=Path(sys.argv[sys.argv.index('--state-dir')+1])\n"
            "s=socket.socket(socket.AF_UNIX);s.bind(str(state/'service.rpc'));s.listen()\n"
            'c,_=s.accept()\nwhile True:\n try:c.sendall(b" ");time.sleep(.02)\n except BrokenPipeError:time.sleep(60)\n')
        started=time.monotonic()
        with self.assertRaisesRegex(ValueError,'timed out'):probe.check(self.runtime,self.root,timeout=.3)
        self.assertLess(time.monotonic()-started,3)
        self.assert_clean()

    def test_interrupt_reaps_sigterm_ignoring_descendant(self):
        heartbeat=self.root/'heartbeat';ready=self.root/'ready'
        child=self.root/'child.py';child.write_text('import signal,time\nfrom pathlib import Path\n'
            'signal.signal(signal.SIGTERM,signal.SIG_IGN)\n'
            f'Path({str(ready)!r}).touch()\n'
            f'with Path({str(heartbeat)!r}).open("a",buffering=1) as output:\n'
            ' while True:output.write("alive\\n");time.sleep(.01)\n')
        self.script.write_text('import subprocess,sys,time\n'+f'subprocess.Popen([sys.executable,{str(child)!r}])\n'+'time.sleep(60)\n')
        real_exchange=probe.exchange
        def interrupt(*args,**kwargs):
            if ready.exists() and heartbeat.exists() and heartbeat.stat().st_size>0:raise KeyboardInterrupt
            return real_exchange(*args,**kwargs)
        with patch.object(probe,'exchange',interrupt):
            with self.assertRaises(KeyboardInterrupt):probe.check(self.runtime,self.root)
        self.assert_clean();before=heartbeat.read_bytes();time.sleep(.1)
        self.assertEqual(before,heartbeat.read_bytes(),'descendant survived cancellation')


if __name__=='__main__':unittest.main()
