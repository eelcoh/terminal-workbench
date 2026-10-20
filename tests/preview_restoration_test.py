"""Restoration never replaces a responding or unverifiable service."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import preview_activation as activation


class RestorationTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-restore-',dir='/tmp');self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.state=self.root/'state';self.state.mkdir(mode=0o700)
        self.payload=self.root/'previous';self.runtime=self.payload/'libexec/workbench-session'
        self.runtime.parent.mkdir(parents=True);self.runtime.touch();self.runtime.chmod(0o755)
        (self.payload/'preview.json').write_text(json.dumps(dict(format=1)))
        self.script=self.payload/'scripts/session_service.py';self.script.parent.mkdir()
        self.script.write_text('raise SystemExit(42)\n')
        self.previous=dict(runtime_binary=str(self.runtime),directory=str(self.root))

    def test_responding_service_is_never_stopped_or_replaced(self):
        with patch.object(activation,'inspect',return_value={'service_instance':'other'}),patch.object(activation.subprocess,'Popen') as spawn:
            result=activation.restore_previous_service(self.state,self.previous)
        spawn.assert_not_called();self.assertIn('responding service was left unchanged',result)

    def test_unverifiable_service_does_not_trigger_restoration(self):
        with patch.object(activation,'inspect',side_effect=ValueError('incompatible reply')),patch.object(activation.subprocess,'Popen') as spawn:
            result=activation.restore_previous_service(self.state,self.previous)
        spawn.assert_not_called();self.assertIn('status could not be verified',result)

    def test_missing_previous_payload_reports_failure(self):
        self.runtime.unlink()
        with patch.object(activation,'inspect',return_value=None),patch.object(activation.subprocess,'Popen') as spawn:
            result=activation.restore_previous_service(self.state,self.previous)
        spawn.assert_not_called();self.assertIn('could not be restored',result)

    def test_missing_recorded_python_is_reported_without_starting(self):
        (self.payload/'python.path').write_text(str(self.root/'missing-python'))
        with patch.object(activation,'inspect',return_value=None),patch.object(activation.subprocess,'Popen') as spawn:
            result=activation.restore_previous_service(self.state,self.previous)
        spawn.assert_not_called();self.assertIn('Python interpreter is missing',result)

    def test_failed_restoration_reaps_its_child_and_keeps_log(self):
        children=[];original=subprocess.Popen
        def start(*args,**kwargs):
            child=original(*args,**kwargs);children.append(child);return child
        with patch.object(activation,'inspect',return_value=None),patch.object(activation.subprocess,'Popen',start):
            result=activation.restore_previous_service(self.state,self.previous)
        self.assertIn('could not be restored',result);self.assertTrue(children)
        self.assertTrue(all(child.poll() is not None for child in children))
        self.assertTrue((self.state/'service.log').is_file())


if __name__=='__main__':unittest.main()
