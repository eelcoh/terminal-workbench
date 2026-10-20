"""Copied-preview verification owns diagnostics and cannot activate or open a window."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from preview_install_check import verify,last_check


class InstallCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-install-check-');self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.payload=self.root/'payload';self.payload.mkdir()
        self.launcher=self.payload/'terminal-workbench'

    def script(self,body):
        self.launcher.write_text('#!'+sys.executable+'\nimport json,sys,time,os\nfrom pathlib import Path\n'+body+'\n')
        self.launcher.chmod(0o755)

    def result(self,**extra):
        return dict(schema=1,ok=True,payload=str(self.payload),checks=[dict(name='qml_startup',ok=True)],**extra)

    def test_success_preserves_payload_and_separates_json_from_stderr(self):
        self.script("assert sys.argv[1:]==['--check']\nprint('diagnostic stderr',file=sys.stderr)\nprint("+repr(json.dumps(self.result()))+")")
        before=self.launcher.read_bytes();receipt=verify(self.root,self.launcher)
        value=json.loads(receipt.read_text());self.assertTrue(value['ok']);self.assertEqual(value['report']['payload'],str(self.payload))
        self.assertIn('diagnostic stderr',(self.root/'installation-check.log').read_text())
        self.assertEqual(json.loads((self.root/'installation-check.stdout').read_text())['schema'],1)
        self.assertEqual(self.launcher.read_bytes(),before);self.assertEqual(set(self.payload.iterdir()),{self.launcher})
        self.assertEqual(receipt.stat().st_mode&0o077,0)
        self.assertEqual(last_check(self.root,self.payload)['result'],'passed')
        self.assertFalse(list(self.root.glob('.installation-check-*')))

    def test_failed_malformed_and_wrong_payload_results_keep_evidence(self):
        old=self.root/'older';old.mkdir();sentinel=old/'retained';sentinel.write_text('old preview')
        cases=[("print('not JSON')",None),("print("+repr(json.dumps(dict(self.result(),ok=False)))+")",None),
            ("print("+repr(json.dumps(dict(self.result(),checks=[])))+")",None),
            ("print("+repr(json.dumps(dict(self.result(),checks=[dict(ok=False)])))+")",None),
            ("print("+repr(json.dumps(dict(self.result(),payload='/other/payload')))+")",None),
            ("print("+repr(json.dumps(self.result()))+")\nsys.exit(42)",None)]
        for body,_ in cases:
            with self.subTest(body=body):
                self.script(body)
                with self.assertRaisesRegex(ValueError,'Installed copy retained'):verify(self.root,self.launcher)
                self.assertFalse(json.loads((self.root/'installation-check.json').read_text())['ok'])
                self.assertEqual(last_check(self.root,self.payload)['result'],'failed')
                self.assertTrue(self.launcher.exists());self.assertTrue((self.root/'installation-check.stdout').exists())
                self.assertEqual(sentinel.read_text(),'old preview')

    def test_timeout_reaps_the_check_and_records_failure(self):
        pidfile=self.root/'pid'
        self.script('Path('+repr(str(pidfile))+').write_text(str(os.getpid()))\ntime.sleep(20)')
        with self.assertRaises(ValueError):verify(self.root,self.launcher,timeout=.25)
        self.assertEqual(last_check(self.root,self.payload)['result'],'failed')
        with self.assertRaises(ProcessLookupError):os.kill(int(pidfile.read_text()),0)

    def test_old_missing_corrupt_or_relocated_receipts_are_not_fresh_success(self):
        self.assertEqual(last_check(self.root,self.payload),dict(result='not-run'))
        path=self.root/'installation-check.json';path.write_text('{')
        self.assertEqual(last_check(self.root,self.payload)['result'],'unavailable')
        path.write_text(json.dumps(dict(schema=1,ok=True,launcher='/previous/path/terminal-workbench')))
        self.assertEqual(last_check(self.root,self.payload)['result'],'unavailable')
        path.unlink();outside=self.root/'outside';outside.write_text('{}');path.symlink_to(outside)
        self.assertEqual(last_check(self.root,self.payload)['result'],'unavailable')


if __name__=='__main__':unittest.main()
