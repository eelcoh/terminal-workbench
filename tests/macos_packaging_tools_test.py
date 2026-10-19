"""Real packaging subprocess deadlines, progress, and cancellation isolation."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from platform_launchers import macos_tools as tools


class ToolTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb package tool ')
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        self.script=self.root/'tool.py'
        self.log=self.root/'tool.log'

    def invoke(self,source,**kwargs):
        self.script.write_text(source)
        with self.log.open('w') as output:
            return tools.run_tool([sys.executable,str(self.script)],output,'Fixture packaging',self.log,**kwargs)

    def test_snapshot_only_contains_packaging_process_and_descendants(self):
        text='10 1 S 00:01 /opt/Qt Tools/macdeployqt\n11 10 S 00:01 /Qt/qmlimportscanner\n12 11 R 00:01 /usr/bin/otool\n20 1 S 01:00 /unrelated/private app\n'
        rows=tools._descendants(text,10)
        self.assertEqual(len(rows),3)
        self.assertTrue(any('qmlimportscanner' in row for row in rows))
        self.assertFalse(any('private app' in row for row in rows))

    def test_success_announces_log_and_keeps_tool_output(self):
        messages=[]
        self.invoke("print('Qt fixture output',flush=True)\n",report=messages.append)
        self.assertIn(str(self.log),messages[0])
        self.assertIn('finished',messages[-1])
        self.assertIn('Qt fixture output',self.log.read_text())

    def test_timeout_reports_waiting_and_reaps_process(self):
        messages=[];observed=[]
        original=tools.subprocess.Popen
        def spawn(*args,**kwargs):
            process=original(*args,**kwargs);observed.append(process);return process
        with patch.object(tools.subprocess,'Popen',spawn):
            with self.assertRaises(subprocess.TimeoutExpired):
                self.invoke('import time\ntime.sleep(60)\n',timeout=.2,heartbeat=.05,report=messages.append)
        self.assertTrue(any('still running' in message for message in messages))
        self.assertIsNotNone(observed[0].returncode)
        self.assertIn('Packaging process snapshot',self.log.read_text())
        self.assertIn(str(observed[0].pid),self.log.read_text())
        self.assertFalse(any('finished' in message for message in messages))

    def test_interrupt_stops_term_ignoring_child_before_returning(self):
        child=self.root/'child.py';heartbeat=self.root/'heartbeat';ready=self.root/'ready'
        child.write_text('import os,signal,time\nfrom pathlib import Path\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\n'
            f'Path({str(ready)!r}).write_text(str(os.getpid()))\n'
            f'with Path({str(heartbeat)!r}).open("a",buffering=1) as output:\n'
            ' while True:\n  output.write("alive\\n");time.sleep(.02)\n')
        parent='import subprocess,sys,time\n'+f'subprocess.Popen([sys.executable,{str(child)!r}])\n'+'time.sleep(60)\n'
        def interrupt(message):
            if 'still running' in message and ready.exists():raise KeyboardInterrupt
        try:
            with self.assertRaises(KeyboardInterrupt):self.invoke(parent,timeout=10,heartbeat=.05,report=interrupt)
            before=heartbeat.read_bytes();time.sleep(.15)
            self.assertEqual(before,heartbeat.read_bytes(),'packaging child survived cancellation')
        finally:
            # Cleanup even when this regression fails.
            if ready.exists():
                try:os.kill(int(ready.read_text()),signal.SIGKILL)
                except ProcessLookupError:pass

    def test_nonzero_exit_is_not_reported_as_success(self):
        messages=[]
        with self.assertRaises(subprocess.CalledProcessError):self.invoke('raise SystemExit(23)\n',report=messages.append)
        self.assertFalse(any('finished' in message for message in messages))


if __name__=='__main__':unittest.main()
